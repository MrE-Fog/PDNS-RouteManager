#include "StateSaver.h"
#include "RouteInfo.pb.h"

StateSaver::StateSaver(ILogger &_logger, const std::string &_filename, const int _saveInterval, const int _sleepMS):
    logger(_logger),
    filename(_filename),
    saveInterval(_saveInterval),
    sleepMS(_sleepMS)
{
    shutdownRequested.store(false);
}

void StateSaver::Worker()
{
    logger.Info()<<"Starting-up state saver, target file: "<<filename<<std::endl;
    logger.Info()<<"Shuting down state saver and flusing state-file"<<std::endl;
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
