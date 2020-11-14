#include "StateSaver.h"
#include "RouteInfo.pb.h"

#include <chrono>
#include <thread>

StateSaver::StateSaver(ILogger &_logger, const std::string &_filename, const int _saveInterval, const int _sleepMS):
    logger(_logger),
    filename(_filename),
    saveInterval(_saveInterval),
    sleepMS(_sleepMS)
{
    shutdownRequested.store(false);
    state=0;
}

void StateSaver::SaveRoutes(int)
{
    {
        const std::lock_guard<std::mutex> lock(opLock);
        if(state==0)
            return;
        if(state==1)
        {
            //rewrite file

            //switch to append mode
            state=2;
        }
        else if(state==2)
        {
            //append new routes to the end of file
        }
        //generate list of routes to process
    }

    //save prepared list of routes
    //close file
}

void StateSaver::Worker()
{
    logger.Info()<<"Starting-up state saver, target file: "<<filename<<std::endl;
    while(!shutdownRequested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepMS));
        SaveRoutes(1);
    }
    logger.Info()<<"Shuting down state saver and flusing state-file"<<std::endl;
}

void StateSaver::OnShutdown()
{
    shutdownRequested.store(true);
}

bool StateSaver::ReadyForMessage(const MsgType msgType)
{
    return msgType==MSG_SAVE_ROUTE;
}

void StateSaver::OnMessage(const IMessage&)
{
}
