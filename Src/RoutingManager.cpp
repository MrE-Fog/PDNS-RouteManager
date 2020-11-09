#include "RoutingManager.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };

//should be a POD type
struct RouteMsg
{
    public:
        nlmsghdr nl;
        rtmsg rt;
        unsigned char data[64];
};

RoutingManager::RoutingManager(ILogger &_logger, const char* const _ifname, const IPAddress &_gateway4, const IPAddress &_gateway6, const uint _extraTTL, const int _mgIntervalSec, const int _mgPercent, const int _metric, const int _ksMetric):
    logger(_logger),
    ifname(_ifname),
    gateway4(_gateway4),
    gateway6(_gateway6),
    extraTTL(_extraTTL),
    mgIntervalSec(_mgIntervalSec),
    mgPercent(_mgPercent),
    metric(_metric),
    ksMetric(_ksMetric),
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

    if (bind(sock, (sockaddr *)&nlAddr, sizeof(nlAddr)) == -1)
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
    curTime.store((uint64_t)(unsigned)time.tv_sec);
    return (uint64_t)(unsigned)time.tv_sec;
}

void RoutingManager::Worker()
{
    logger.Info()<<"RoutingManager worker starting up"<<std::endl;
    auto prev=curTime.load();
    while (!shutdownPending.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        auto now=_UpdateCurTime();
        if(now-prev>=(unsigned)mgIntervalSec)
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
    _ProcessPendingInserts(); //trigger pending routes processing
}

void RoutingManager::ManageRoutes()
{
    const std::lock_guard<std::mutex> lock(opLock);
    _ProcessPendingInserts(); //process pending routes if any
    //TODO: initiate remove of stale routes, up to max percent
}

#define NLMSG_TAIL(nmsg) ((struct rtattr *) (((unsigned char *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

static void AddRTA(struct nlmsghdr *n, unsigned short type, const void *data, size_t alen)
{
    unsigned short len = (short)(RTA_LENGTH(alen));
    rtattr *rta;
    if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > sizeof(RouteMsg))
        exit(10); //should not happen normally
    rta = NLMSG_TAIL(n);
    rta->rta_type=type;
    rta->rta_len=(short)len;
    if (alen>0)
        memcpy(RTA_DATA(rta), data, alen);
    n->nlmsg_len=NLMSG_ALIGN(n->nlmsg_len)+RTA_ALIGN(len);
}

void RoutingManager::_ProcessPendingInserts()
{
    //refuse to do anything if netlink socket is not initialized
    if(!started)
        return;
    for (auto const &el : pendingInserts)
    {
        //TODO: initiate remove of currently expired routes
        //re-add routes with valid expiration times
        _PushRoute(el.first,true); //push blackhole route first
        if(ifCfg.Get().isUp)
            _PushRoute(el.first,false); //push regular route next
    }
}

void RoutingManager::_PushRoute(const IPAddress &ip, bool blackhole)
{
    if(blackhole)
        logger.Info()<<"Pushing blackhole rule for: "<<ip<<std::endl;
    else
        logger.Info()<<"Pushing routing rule for: "<<ip<<std::endl;

    RouteMsg msg={};

    msg.nl.nlmsg_len=NLMSG_LENGTH(sizeof(rtmsg));
    msg.nl.nlmsg_flags=NLM_F_REQUEST|NLM_F_CREATE|NLM_F_REPLACE;
    msg.nl.nlmsg_type=RTM_NEWROUTE;

    msg.rt.rtm_table=RT_TABLE_MAIN;
    msg.rt.rtm_scope=RT_SCOPE_UNIVERSE;
    msg.rt.rtm_type=blackhole?RTN_BLACKHOLE:RTN_UNICAST;
    //msg.rt.rtm_flags=RTM_F_NOTIFY;
    msg.rt.rtm_protocol=RTPROT_STATIC; //TODO: check do we really need this
    msg.rt.rtm_dst_len=ip.isV6?128:32;
    msg.rt.rtm_family=ip.isV6?AF_INET6:AF_INET;

    //add destination
    unsigned char addr[IP_ADDR_LEN]={};
    ip.ToBinary(addr);
    AddRTA(&msg.nl,RTA_DST,addr,ip.isV6?IPV6_ADDR_LEN:IPV4_ADDR_LEN);

    if(!blackhole)
    {
        //add interface
        auto ifIdx=if_nametoindex(ifname);
        AddRTA(&msg.nl,RTA_OIF,&ifIdx,sizeof(ifIdx));
    }

    //set metric/priority
    int prio=blackhole?ksMetric:metric;
    AddRTA(&msg.nl,RTA_PRIORITY,&prio,sizeof(prio));

    //add gateway
    if(!blackhole && !ifCfg.Get().isPtP)
    {
        if(gateway4.isValid && !ip.isV6)
        {
            unsigned char gw[IP_ADDR_LEN]={};
            gateway4.ToBinary(gw);
            AddRTA(&msg.nl,RTA_GATEWAY,gw,IPV4_ADDR_LEN);
        }
        if(gateway6.isValid && ip.isV6)
        {
            unsigned char gw[IP_ADDR_LEN]={};
            gateway6.ToBinary(gw);
            AddRTA(&msg.nl,RTA_GATEWAY,gw,IPV6_ADDR_LEN);
        }
    }

    //send netlink message:
    if(send(sock,&msg,sizeof(RouteMsg),0)!=sizeof(RouteMsg))
        logger.Error()<<"Failed to send route via netlink: "<<strerror(errno)<<std::endl;
}

void RoutingManager::InsertRoute(const IPAddress& dest, uint ttl)
{
    const std::lock_guard<std::mutex> lock(opLock);
    auto expirationTime=curTime.load()+ttl+extraTTL;

    //check, maybe we already have this route as active
    auto aIT=activeRoutes.find(dest);
    if(activeRoutes.find(dest)!=activeRoutes.end())
    {
        //if so - update expiration time, and return
        if(aIT->second<expirationTime)
        {
            logger.Info()<<"Already installed route-rule detected, updating expiration time for: "<<dest<<std::endl;
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
        _PushRoute(dest,true);
        //push new route immediately, only if network is up and running
        if(ifCfg.Get().isUp)
            _PushRoute(dest,false);
    }

    //add new pending route, or update it's expiration time
    auto pIT=pendingInserts.find(dest);
    if(pIT==pendingInserts.end()||pIT->second<expirationTime)
        pendingInserts[dest]=expirationTime;
}

void RoutingManager::ConfirmRoute(const IPAddress &dest)
{
    const std::lock_guard<std::mutex> lock(opLock);
    logger.Info()<<"Processing route-added confirmation for: "<<dest<<std::endl;
    //if there are no pendingInserts record for this IP, show warning
    auto pIT=pendingInserts.find(dest);
    uint64_t expiration=curTime.load()+extraTTL;
    if(pIT==pendingInserts.end())
        logger.Warning()<<"No pending route-rule insert found for: "<<dest<<std::endl;
    else
    {
        expiration=pIT->second;
        pendingInserts.erase(dest);
    }
    //move rule to activeRoutes, or create rule with minumum allowed ttl
    activeRoutes[dest]=expiration;
    pendingExpires.insert({expiration,dest});
}

bool RoutingManager::ReadyForMessage(const MsgType msgType)
{
    return (!shutdownPending.load())&&(msgType==MSG_NETDEV_UPDATE||msgType==MSG_ROUTE_REQUEST||msgType==MSG_ROUTE_ADDED||msgType==MSG_ROUTE_REMOVED||msgType==MSG_INIT_ROUTES);
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
        ConfirmRoute(addMsg.ip);
        return;
    }

    if(message.msgType==MSG_INIT_ROUTES)
    {
        //TODO: install initial routes-list early after startup
        return;
    }
}
