#include "RoutingManager.h"

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };

//background worker, will do periodical routes cleanup and update current clock-value
RoutingManager::RoutingManager(ILogger &_logger, const char* const _ifname, const IPAddress _gateway, const uint _extraTtl, const int _mgIntervalSec, const int _mgPercent):
    logger(_logger),
    ifname(_ifname),
    gateway(_gateway),
    extraTtl(_extraTtl),
    mgIntervalSec(_mgIntervalSec),
    mgPercent(_mgPercent)
{
    shutdownPending.store(false);
    socket=-1;
}

//overrodes for performing some extra-init
bool RoutingManager::Startup()
{
    //TODO: open netlink socket
    //TODO: set initial clock-value

    //start background worker that will do periodical cleanup
    return WorkerBase::Startup();
}

bool RoutingManager::Shutdown()
{
    //stop background worker
    return WorkerBase::Shutdown();

    //close netlink socket
}

//will be called by WorkerBase::Shutdown() or WorkerBase::RequestShutdown()
void RoutingManager::OnShutdown()
{
    shutdownPending.store(true);
}



void RoutingManager::Worker()
{

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
