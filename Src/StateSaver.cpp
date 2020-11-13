#include "StateSaver.h"
#include "RouteInfo.pb.h"

StateSaver::StateSaver(ILogger &_logger):
    logger(_logger)
{
    shutdownRequested.store(false);
}

void StateSaver::Worker()
{

}

void StateSaver::OnShutdown()
{
    shutdownRequested.store(true);
}

bool StateSaver::ReadyForMessage(const MsgType msgType)
{
    return (msgType==MSG_ROUTE_REQUEST||msgType==MSG_ROUTE_ADDED||msgType==MSG_ROUTE_REMOVED);
}

void StateSaver::OnMessage(const IMessage& message)
{

}
