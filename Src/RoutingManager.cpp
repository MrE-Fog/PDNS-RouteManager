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

RoutingManager::RoutingManager(ILogger &_logger, const char* const _ifname, const IPAddress _gateway, const uint _extraTtl, const int _mgIntervalSec, const int _mgPercent, const int _metric, const int _ksMetric):
    logger(_logger),
    ifname(_ifname),
    gateway(_gateway),
    extraTtl(_extraTtl),
    mgIntervalSec(_mgIntervalSec),
    mgPercent(_mgPercent),
    metric(_metric),
    ksMetric(_ksMetric)
{
    UpdateCurTime();
    shutdownPending.store(false);
    sock=-1;
    seqNum=0;
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

uint64_t RoutingManager::UpdateCurTime()
{
    const std::lock_guard<std::mutex> lock(opLock);
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
        auto now=UpdateCurTime();
        if(now-prev>=(unsigned)mgIntervalSec)
        {
            prev=now;
            CleanStaleRoutes();
        }
    }
    logger.Info()<<"Shuting down RoutingManager worker"<<std::endl;
}

void RoutingManager::CleanStaleRoutes()
{
    const std::lock_guard<std::mutex> lock(opLock);
    //TODO: clean stale routes, up to max percent
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
        //if network changed to UP state - reinstall all currently initiated rules
        //suspend any real route-update operations if network changed to DOWN state, but continue to track incoming routes
    }
    else if(message.msgType==MSG_ROUTE_REQUEST)
    {
        //track new route and push it to ther kernel if network state is up and running
    }
}
