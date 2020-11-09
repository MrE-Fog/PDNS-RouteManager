#include "ILogger.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"
#include "MessageBroker.h"
#include "SignalHandler.h"
#include "ShutdownHandler.h"

#include <iostream>
#include <thread>
#include <chrono>

/**/



int main (int argc, char *argv[])
{
    const int timeoutMs=500; //TODO: read timeouts from config
    const timeval timeoutTv = { timeoutMs/1000, (timeoutMs-timeoutMs/1000*1000)*1000 };

    StdioLogger logger;
    if(argc!=4)
    {
        logger.Error() << "Usage: " << argv[0] << " <listen ip-addr> <port> <target netdev>" << std::endl;
        return 1;
    }

    //configure essential stuff
    MessageBroker messageBroker;
    SignalHandler::Setup();
    ShutdownHandler control;
    messageBroker.AddSubscriber(control);

    //create main worker-instances
    NetDevTracker tracker(logger,messageBroker,timeoutTv,argv[3]);

    //init
    tracker.Startup();

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if(control.IsShutdownRequested())
        {
            if(control.GetEC()!=0)
                logger.Error() << "One of background worker was failed, shuting down" << std::endl;
            else
                logger.Info() << "Shuting down gracefully by signal from background worker" << std::endl;
            break;
        }
        if(SignalHandler::IsSignalReceived())
            break;
    }

    //shutdown
    tracker.Shutdown();

    return  0;
}

