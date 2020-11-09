#include "ILogger.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"
#include "DNSReceiver.h"
#include "MessageBroker.h"
#include "ShutdownHandler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <string>

int main (int argc, char *argv[])
{
    //set timeouts used by background workers for network operations and some other events
    //increasing this time will slow down reaction to some internal and external events
    //decreasing this time too much will cause high cpu usage
    const int timeoutMs=500;
    const timeval timeoutTv={timeoutMs/1000,(timeoutMs-timeoutMs/1000*1000)*1000};

    //timeout for main thread waiting for external signals
    const timespec sigTs={2,0};

    StdioLogger logger;

    //TODO: add sane option parsing
    if(argc<4)
    {
        logger.Error() << "Usage: " << argv[0] << " <listen ip-addr> <port> <target netdev> [metric] [use byte swap: true|false] [gateway ip] [extratTTL] [management interval] [max percent of routes to manage in a single management operation]" << std::endl;
        return 1;
    }

    //parse port
    if(std::strlen(argv[2])>5)
    {
        logger.Error() << "port number is too long!" << std::endl;
        return 1;
    }

    //parse optional parameters
    const int metric=(argc>4)?std::atoi(argv[4]):100;
    const bool useByteSwap=(argc>5)?(std::strncmp(argv[5],"true",4)==0):false;
    const IPAddress gateway=(argc>6)?IPAddress(argv[6]):IPAddress("0.0.0.0");
    const uint extraTTL=(argc>7)?(uint)std::atoi(argv[7]):(uint)(120*60);
    const int mgIntervalSec=(argc>8)?std::atoi(argv[8]):5;
    const int mgPercent=(argc>9)?std::atoi(argv[9]):5;

    if(metric<1)
    {
        logger.Error() << "metric number is invalid!" << std::endl;
        return 1;
    }

    if(!gateway.isValid)
    {
        logger.Error() << "gateway address is invalid!" << std::endl;
        return 1;
    }

    if(extraTTL<1)
    {
        logger.Error() << "extraTTL time is invalid!" << std::endl;
        return 1;
    }

    if(mgIntervalSec<1)
    {
        logger.Error() << "management interval time is invalid!" << std::endl;
        return 1;
    }

    if(mgPercent<1 || mgPercent>100)
    {
        logger.Error() << "management percent value is invalid!" << std::endl;
        return 1;
    }

    //configure essential stuff
    MessageBroker messageBroker;
    ShutdownHandler shutdownHandler;
    messageBroker.AddSubscriber(shutdownHandler);

    //create main worker-instances
    DNSReceiver dnsReceiver(logger,messageBroker,timeoutTv,IPAddress(argv[1]),std::atoi(argv[2]),useByteSwap);
    NetDevTracker tracker(logger,messageBroker,timeoutTv,argv[3]);

    //create sigset_t struct with signals
    sigset_t sigset;
    sigemptyset(&sigset);
    if(sigaddset(&sigset,SIGHUP)!=0||sigaddset(&sigset,SIGTERM)!=0||sigaddset(&sigset,SIGUSR1)!=0||sigaddset(&sigset,SIGUSR2)!=0||pthread_sigmask(SIG_BLOCK,&sigset,nullptr)!=0)
    {
        logger.Error()<<"Failed to setup signal-handling"<<std::endl;
        return 1;
    }

    //start background workers, or perform post-setup init
    dnsReceiver.Startup();
    tracker.Startup();

    while(true)
    {
        auto signal=sigtimedwait(&sigset,nullptr,&sigTs);
        auto error=errno;
        if(signal<0 && error!=EAGAIN && error!=EINTR)
        {
            logger.Error()<<"Error while handling incoming signal: "<<strerror(error)<<std::endl;
            break;
        }
        else if(signal>0 && signal!=SIGUSR2 && signal!=SIGINT) //SIGUSR2 triggered by shutdownhandler to unblock sigtimedwait
        {
            logger.Info()<< "Pending shutdown by receiving signal: "<<signal<<"->"<<strsignal(signal)<<std::endl;
            break;
        }

        if(shutdownHandler.IsShutdownRequested())
        {
            if(shutdownHandler.GetEC()!=0)
                logger.Error() << "One of background worker was failed, shuting down" << std::endl;
            else
                logger.Info() << "Shuting down gracefully by request from background worker" << std::endl;
            break;
        }
    }

    //request shutdown of background workers
    dnsReceiver.RequestShutdown();
    tracker.RequestShutdown();

    //wait for background workers shutdown complete
    dnsReceiver.Shutdown();
    tracker.Shutdown();

    return  0;
}

