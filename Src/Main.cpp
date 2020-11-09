#include "ILogger.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"
#include "DNSReceiver.h"
#include "MessageBroker.h"
#include "SignalHandler.h"
#include "ShutdownHandler.h"
#include "ProtobufHelper.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>


int main (int argc, char *argv[])
{
    const int timeoutMs=300; //TODO: read timeouts from config
    const timeval timeoutTv = { timeoutMs/1000, (timeoutMs-timeoutMs/1000*1000)*1000 };

    StdioLogger logger;

    //TODO: add sane option parsing
    if(argc<4)
    {
        logger.Error() << "Usage: " << argv[0] << " <listen ip-addr> <port> <target netdev> [use byte swap: true|false]" << std::endl;
        return 1;
    }

    //parse port
    if(std::strlen(argv[2])>5)
    {
        logger.Error() << "port number is too long!" << std::endl;
        return 1;
    }

    bool useByteSwap=false;
    if(argc>4)
        useByteSwap=(std::strncmp(argv[4],"true",4)==0);

    //test protobuf
   // ProtobufHelper pbHelper(logger,useByteSwap);
   // pbHelper.Test("../../test1.pb");

    //configure essential stuff
    MessageBroker messageBroker;
    SignalHandler::Setup();
    ShutdownHandler shutdownHandler;
    messageBroker.AddSubscriber(shutdownHandler);

    //create main worker-instances
    DNSReceiver dnsReceiver(logger,messageBroker,timeoutTv,IPAddress(argv[1]),std::atoi(argv[2]),useByteSwap);
    NetDevTracker tracker(logger,messageBroker,timeoutTv,argv[3]);

    //start background workers, or perform post-setup init
    dnsReceiver.Startup();
    tracker.Startup();

    while(true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if(shutdownHandler.IsShutdownRequested())
        {
            if(shutdownHandler.GetEC()!=0)
                logger.Error() << "One of background worker was failed, shuting down" << std::endl;
            else
                logger.Info() << "Shuting down gracefully by signal from background worker" << std::endl;
            break;
        }
        if(SignalHandler::IsSignalReceived())
            break;
    }

    //shutdown background workers
    dnsReceiver.Shutdown();
    tracker.Shutdown();

    return  0;
}

