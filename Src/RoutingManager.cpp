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
struct RouteRequest
{
    public:
        nlmsghdr nl;
        rtmsg rt;
        unsigned char data[4096];
};

RoutingManager::RoutingManager(ILogger &_logger, const char* const _ifname, const IPAddress _gateway, const uint _extraTTL, const int _mgIntervalSec, const int _mgPercent, const int _metric, const int _ksMetric):
    logger(_logger),
    ifname(_ifname),
    gateway(_gateway),
    extraTTL(_extraTTL),
    mgIntervalSec(_mgIntervalSec),
    mgPercent(_mgPercent),
    metric(_metric),
    ksMetric(_ksMetric),
    ifCfg(ImmutableStorage<InterfaceConfig>(InterfaceConfig()))
{
    _UpdateCurTime();
    shutdownPending.store(false);
    sock=-1;
    seqNum=10;
}

//overrodes for performing some extra-init
bool RoutingManager::Startup()
{
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

    //TODO: set initial clock-value

    //start background worker that will do periodical cleanup
    return WorkerBase::Startup();
}

bool RoutingManager::Shutdown()
{
    //stop background worker
    auto result=WorkerBase::Shutdown();

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
            CleanStaleRoutes();
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

void RoutingManager::CleanStaleRoutes()
{
    const std::lock_guard<std::mutex> lock(opLock);
    //TODO: clean stale routes, up to max percent
}

uint32_t RoutingManager::_UpdateSeqNum()
{
    seqNum++;
    if(seqNum<10)
        seqNum=10;
    return seqNum;
}

void RoutingManager::_ProcessPendingInserts()
{
    //TODO
}

void RoutingManager::InsertRoute(const IPAddress& dest, uint ttl)
{
    const std::lock_guard<std::mutex> lock(opLock);

    //TODO: check, maybe we already have this route as pending
    //if so - update pending route expiration time, generate new sequence number and process pending route insertion
    //and return

    //TODO: check, maybe we already have this route as active
    //if so - update expiration time
    //and return

    //add new pending route
    auto newRoute=Route(dest,curTime.load()+ttl+extraTTL);
    auto newSeq=_UpdateSeqNum();
    pendingInserts.insert({newSeq,newRoute});

    //trigger pending routes processing
    _ProcessPendingInserts();
}


bool RoutingManager::ReadyForMessage(const MsgType msgType)
{
    return (!shutdownPending.load())&&(msgType==MSG_NETDEV_UPDATE || msgType==MSG_ROUTE_REQUEST);
}

//main logic executed from message handler
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
}
