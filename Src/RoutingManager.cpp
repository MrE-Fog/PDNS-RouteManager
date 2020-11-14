#include "RoutingManager.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <vector>

#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };

//MUST be a POD type
struct RouteMsg
{
    public:
        nlmsghdr nl;
        rtmsg rt;
        unsigned char data[64];
};

RoutingManager::RoutingManager(ILogger &_logger, const std::string &_ifname, const IPAddress &_gateway4, const IPAddress &_gateway6, const unsigned int _extraTTL, const int _mgIntervalSec, const int _mgPercent, const int _metric, const int _ksMetric, const int _addRetryCount):
    logger(_logger),
    ifname(_ifname),
    gateway4(_gateway4),
    gateway6(_gateway6),
    extraTTL(_extraTTL),
    mgIntervalSec(_mgIntervalSec),
    mgPercent(_mgPercent),
    metric(_metric),
    ksMetric(_ksMetric),
    addRetryCount(_addRetryCount),
    ifCfg(ImmutableStorage<InterfaceConfig>(InterfaceConfig()))
{
    _UpdateCurTime();
    shutdownPending.store(false);
    started=false;
    sock=-1;
}

//overrodes for performing some extra-init
bool RoutingManager::Startup()
{
    const std::lock_guard<std::mutex> lock(opLock);

    //open netlink socket
    logger.Info()<<"Preparing RoutingManager for interface: "<<ifname<<std::endl;

    sock=socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(sock==-1)
    {
        logger.Error()<<"Failed to open netlink socket: "<<strerror(errno)<<std::endl;
        return false;
    }

    sockaddr_nl nlAddr = {};
    nlAddr.nl_family=AF_NETLINK;
    nlAddr.nl_groups=0;

    if (bind(sock, reinterpret_cast<sockaddr*>(&nlAddr), sizeof(nlAddr)) == -1)
    {
        logger.Error()<<"Failed to bind to netlink socket: "<<strerror(errno)<<std::endl;
        return false;
    }

    started=true;

    //start background worker that will do periodical cleanup
    return WorkerBase::Startup();
}

bool RoutingManager::Shutdown()
{
    //stop background worker
    auto result=WorkerBase::Shutdown();

    const std::lock_guard<std::mutex> lock(opLock);
    started=false;
    //close netlink socket
    if(close(sock)!=0)
    {
        logger.Error()<<"Failed to close netlink socket: "<<strerror(errno)<<std::endl;
        return false;
    }
    return result;
}

//will be called by WorkerBase::Shutdown() or WorkerBase::RequestShutdown()
void RoutingManager::OnShutdown()
{
    shutdownPending.store(true);
}

uint64_t RoutingManager::_UpdateCurTime()
{
    timespec time={};
    clock_gettime(CLOCK_MONOTONIC,&time);
    curTime.store(static_cast<uint64_t>(static_cast<unsigned>(time.tv_sec)));
    return static_cast<uint64_t>(static_cast<unsigned>(time.tv_sec));
}

void RoutingManager::Worker()
{
    logger.Info()<<"RoutingManager worker starting up"<<std::endl;
    auto prev=curTime.load();
    while (!shutdownPending.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now=_UpdateCurTime();
        if(now-prev>=static_cast<uint64_t>(mgIntervalSec))
        {
            prev=now;
            ManageRoutes();
        }
    }
    logger.Info()<<"Shuting down RoutingManager worker"<<std::endl;
}

void RoutingManager::ProcessNetDevUpdate(const InterfaceConfig& newConfig)
{
    const std::lock_guard<std::mutex> lock(opLock);
    ifCfg.Set(newConfig); //update config
    _ProcessPendingInserts(); //trigger pending routes processing immediately
}

void RoutingManager::ManageRoutes()
{
    const std::lock_guard<std::mutex> lock(opLock);
    _ProcessPendingInserts();
    _ProcessStaleRoutes();
}

#define NLMSG_TAIL(nmsg) ((reinterpret_cast<unsigned char*>(nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len))

static void AddRTA(struct nlmsghdr *n, unsigned short type, const void *data, size_t dataLen)
{
    unsigned short rtaLen = static_cast<unsigned short>(RTA_LENGTH(dataLen));
    //check that we have place to add new attribute
    if ((NLMSG_ALIGN(n->nlmsg_len)+RTA_ALIGN(rtaLen))>sizeof(RouteMsg))
        exit(10); //should not happen if RouteMsg::data is big enough
    //create header for new attribute
    rtattr rtaHDR;
    rtaHDR.rta_type=type;
    rtaHDR.rta_len=rtaLen;
    //copy attribute header to the proper position at the tail of RouteMsg
    auto tail=NLMSG_TAIL(n);
    memcpy(reinterpret_cast<void*>(tail),reinterpret_cast<void*>(&rtaHDR),sizeof(rtattr));
    //copy attribute data after the header
    memcpy(RTA_DATA(tail), data, dataLen);
    //set new total lenght nlmsghdr header
    n->nlmsg_len=NLMSG_ALIGN(n->nlmsg_len)+RTA_ALIGN(rtaLen);
}

void RoutingManager::_ProcessPendingInserts()
{
    //refuse to do anything if netlink socket is not initialized
    if(!started)
        return;

    //if config is unavailable - do nothing
    auto cfg=ifCfg.Get();
    if(!cfg.isUp)
        return;

    auto ipv4Avail=cfg.isIPV4Avail();
    auto ipv6Avail=cfg.isIPV6Avail();

    if(ipv4Avail||ipv6Avail)
    {
        //get list of expired pendingRetries
        std::vector<IPAddress> expiredRetries;
        for (auto const &el : pendingRetries)
            if(el.second>=addRetryCount&&((!el.first.isV6&&ipv4Avail)||(el.first.isV6&&ipv6Avail)))
                expiredRetries.push_back(el.first);
        //consider all expired retries as activated - we do all we can to install that routes
        for (auto const &el : expiredRetries)
        {
            logger.Warning()<<"Giving up on receiving route-added confirmation for: "<<el<<std::endl;
            _FinalizeRouteInsert(el);
        }
    }

    //re-add pending routes
    for (auto const &el : pendingInserts)
    {
        if((!el.first.isV6&&!ipv4Avail)||(el.first.isV6&&!ipv6Avail))
            continue;
        //(re)push blackhole route to make the killswitch that will work if tracked-interface is down
        _ProcessRoute(el.first,true,true);
        //increase retry-counter
        auto rIT=pendingRetries.find(el.first);
        auto insertTry=(rIT==pendingRetries.end())?2:rIT->second+1;
        pendingRetries[el.first]=insertTry;
        //push actual route-rule only if network is running
        logger.Info()<<"Retrying push routing rule for: "<<el.first<<" try: "<<insertTry<<std::endl;
        _ProcessRoute(el.first,false,true);
    }
}

void RoutingManager::_FinalizeRouteInsert(const IPAddress& dest)
{
    //if there are no pendingInserts record for this IP, show warning
    uint64_t expiration=curTime.load()+extraTTL;
    auto pIT=pendingInserts.find(dest);
    if(pIT==pendingInserts.end())
    {
        //try active route
        auto aIT=activeRoutes.find(dest);
        if(aIT!=activeRoutes.end())
        {
            logger.Warning()<<"Ignoring modification of active route expiration time for: "<<dest<<std::endl;
            return;
        }
        //route without pending-insert might be created with minimum ttl
        logger.Warning()<<"No pending route-rule insert found for: "<<dest<<std::endl;
    }
    else
    {
        expiration=pIT->second;
        pendingInserts.erase(pIT);
        pendingRetries.erase(dest);
    }
    activeRoutes[dest]=expiration; //move rule to activeRoutes
    pendingExpires.insert({expiration,dest}); //add pending insert record, for route-management task
}

void RoutingManager::_ProcessRoute(const IPAddress &ip, const bool blackhole, const bool isAddRequest)
{
    RouteMsg msg={};

    msg.nl.nlmsg_len=NLMSG_LENGTH(sizeof(rtmsg));
    msg.nl.nlmsg_flags=isAddRequest?(NLM_F_REQUEST|NLM_F_CREATE|NLM_F_REPLACE):NLM_F_REQUEST;
    msg.nl.nlmsg_type=isAddRequest?RTM_NEWROUTE:RTM_DELROUTE;

    msg.rt.rtm_table=RT_TABLE_MAIN;
    msg.rt.rtm_scope=RT_SCOPE_UNIVERSE;
    msg.rt.rtm_type=blackhole?RTN_BLACKHOLE:RTN_UNICAST;
    //msg.rt.rtm_flags=RTM_F_NOTIFY;
    msg.rt.rtm_protocol=RTPROT_STATIC; //TODO: check do we really need this
    msg.rt.rtm_dst_len=ip.isV6?128:32;
    msg.rt.rtm_family=ip.isV6?AF_INET6:AF_INET;

    //add destination
    AddRTA(&msg.nl,RTA_DST,ip.RawData(),ip.isV6?IPV6_ADDR_LEN:IPV4_ADDR_LEN);

    if(!blackhole)
    {
        //add interface
        auto ifIdx=if_nametoindex(ifname.c_str());
        AddRTA(&msg.nl,RTA_OIF,&ifIdx,sizeof(ifIdx));
    }

    //set metric/priority
    int prio=blackhole?ksMetric:metric;
    AddRTA(&msg.nl,RTA_PRIORITY,&prio,sizeof(prio));

    //add gateway
    if(!blackhole && !ifCfg.Get().isPtP)
    {
        if(gateway4.isValid && !ip.isV6)
            AddRTA(&msg.nl,RTA_GATEWAY,gateway4.RawData(),IPV4_ADDR_LEN);
        if(gateway6.isValid && ip.isV6)
            AddRTA(&msg.nl,RTA_GATEWAY,gateway6.RawData(),IPV6_ADDR_LEN);
    }

    //send netlink message:
    if(send(sock,&msg,sizeof(RouteMsg),0)!=sizeof(RouteMsg))
        logger.Error()<<"Failed to send route via netlink: "<<strerror(errno)<<std::endl;
}

void RoutingManager::_ProcessStaleRoutes()
{
    //get pendingExpires length
    auto expCnt=pendingExpires.size();
    if(expCnt<1)
        return;
    auto remCnt=static_cast<int>(static_cast<float>(expCnt)/100.0f*static_cast<float>(mgPercent));
    if(remCnt<1)
        remCnt=1;
    auto curMark=curTime.load();
    while(remCnt>0)
    {
        auto tIT=pendingExpires.begin();
        if(curMark<tIT->first)
        {
            //logger.Info()<<"time diff for next mark: "<<tIT->first-curMark<<std::endl;
            return;
        }
        logger.Info()<<"Evaluating ip: "<<tIT->second<<" with expire mark: "<<tIT->first<<" current time mark: "<<curMark<<std::endl;
        //check time mark is valid
        auto aIT=activeRoutes.find(tIT->second);
        if(aIT==activeRoutes.end()||aIT->second>tIT->first)
            logger.Info()<<"Removing invalid expire mark: "<<tIT->first<<" for ip: "<<tIT->second<<std::endl;
        else
        {
            _ProcessRoute(aIT->first,false,false); //commence route removal
            logger.Info()<<"Removing expired routing rule for: "<<aIT->first<<" with expite mark: "<<aIT->second<<std::endl;
            _ProcessRoute(aIT->first,true,false); //commence blackhole route removal
            activeRoutes.erase(aIT); //remove from active routes
        }
        //erase pending element
        pendingExpires.erase(tIT);
        remCnt--;
    }
}

void RoutingManager::InsertRoute(const IPAddress& dest, unsigned int ttl)
{
    const std::lock_guard<std::mutex> lock(opLock);
    auto expirationTime=curTime.load()+ttl+extraTTL;

    //check, maybe we already have this route as active
    auto aIT=activeRoutes.find(dest);
    if(aIT!=activeRoutes.end())
    {
        //if so - update expiration time, and return
        if(aIT->second<expirationTime)
        {
            logger.Info()<<"Already installed route-rule detected, updating expiration time: "<<expirationTime<<" for: "<<dest<<std::endl;
            activeRoutes[dest]=expirationTime;
            pendingExpires.insert({expirationTime,dest});
        }
        else
            logger.Warning()<<"Already installed route-rule detected for: "<<dest<<std::endl;
        return;
    }

    //commence netlink operations only if socket is properly started
    if(started)
    {
        //push blackhole route regardless of network state
        _ProcessRoute(dest,true,true);
        auto cfg=ifCfg.Get();
        //push new route immediately, only if network is up and running
        if(cfg.isUp&&((!dest.isV6&&cfg.isIPV4Avail())||(dest.isV6&&cfg.isIPV6Avail())))
        {
            logger.Info()<<"Pushing new routing rule for: "<<dest<<" with expiration time:"<<expirationTime<<std::endl;
            _ProcessRoute(dest,false,true);
        }
        else
            logger.Info()<<"Delaying push new routing rule for: "<<dest<<" with expiration time:"<<expirationTime<<std::endl;
    }

    //add new pending route, or update it's expiration time
    auto pIT=pendingInserts.find(dest);
    if(pIT==pendingInserts.end()||pIT->second<expirationTime)
    {
        pendingInserts[dest]=expirationTime;
        pendingRetries.erase(dest);//cleanup retry counter
    }
}

void RoutingManager::ConfirmRouteAdd(const IPAddress &dest)
{
    const std::lock_guard<std::mutex> lock(opLock);
    logger.Info()<<"Processing route-added confirmation for: "<<dest<<std::endl;
    _FinalizeRouteInsert(dest);
}

void RoutingManager::ConfirmRouteDel(const IPAddress &dest)
{
    const std::lock_guard<std::mutex> lock(opLock);
    logger.Info()<<"Processing route-removed confirmation for: "<<dest<<std::endl;
    //check for unexpected route-removal
    auto aIT=activeRoutes.find(dest);
    if(aIT!=activeRoutes.end())
    {
        logger.Warning()<<"Pending re-add for unexpectedly removed route for: "<<dest<<std::endl;
        pendingInserts.insert({dest,aIT->second});
        pendingRetries.erase(dest);
        activeRoutes.erase(aIT);
    }
}

bool RoutingManager::ReadyForMessage(const MsgType msgType)
{
    return (!shutdownPending.load())&&(msgType==MSG_NETDEV_UPDATE||msgType==MSG_ROUTE_REQUEST||msgType==MSG_ROUTE_ADDED||msgType==MSG_ROUTE_REMOVED);
}

//this logic executed from thread emitting the messages, and must be internally locked
void RoutingManager::OnMessage(const IMessage& message)
{
    if(message.msgType==MSG_NETDEV_UPDATE)
    {
        ProcessNetDevUpdate(static_cast<const INetDevUpdateMessage&>(message).config);
        return;
    }

    if(message.msgType==MSG_ROUTE_REQUEST)
    {
        auto reqMsg=static_cast<const IRouteRequestMessage&>(message);
        InsertRoute(reqMsg.ip,reqMsg.ttl);
        return;
    }

    if(message.msgType==MSG_ROUTE_ADDED)
    {
        auto addMsg=static_cast<const IRouteAddedMessage&>(message);
        ConfirmRouteAdd(addMsg.ip);
        return;
    }

    if(message.msgType==MSG_ROUTE_REMOVED)
    {
        auto rmMsg=static_cast<const IRouteRemovedMessage&>(message);
        ConfirmRouteDel(rmMsg.ip);
        return;
    }
}
