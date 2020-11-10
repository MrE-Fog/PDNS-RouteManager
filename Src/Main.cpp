#include "ILogger.h"
#include "StdioLoggerFactory.h"
#include "RoutingManager.h"
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

void usage(const std::string &self)
{
    std::cerr<<"Usage: "<<self<<" [parameters]"<<std::endl;
    std::cerr<<"  mandatory parameters:"<<std::endl;
    std::cerr<<"    -l <ip-addr> listen ip-address to receive protobuf-encoded DNS packages."<<std::endl;
    std::cerr<<"    -p <port-num> TCP port number to listen at."<<std::endl;
    std::cerr<<"    -t <if-name> network interface that will be used for routing."<<std::endl;
    std::cerr<<"  optional parameters:"<<std::endl;
    std::cerr<<"    -rp <route priority> metric/priority number for generated routes."<<std::endl;
    std::cerr<<"     100 by default. MUST NOT INTERFERE WITH ANY OTHER SYSTEM ROUTES"<<std::endl;
    std::cerr<<"    -bp <blackhole-route priority> metric/priority number for generated"<<std::endl;
    std::cerr<<"     killswitch (blackhole) protective routes, 101 by default."<<std::endl;
    std::cerr<<"     MUST NOT INTERFERE WITH ANY OTHER ROUTES and must be higher than -rp"<<std::endl;
    std::cerr<<"    -s <true|false> swap bytes when decoding protobuf messages."<<std::endl;
    std::cerr<<"     true if PDNS service is running on architecture with different endianness"<<std::endl;
    std::cerr<<"    -gw4 <ip-addr> ipv4 gateway address. not used with p-t-p interfaces"<<std::endl;
    std::cerr<<"    -gw6 <ip-addr> ipv6 gateway address. not used with p-t-p interfaces"<<std::endl;
    std::cerr<<"    -ttl <seconds> additional time interval added to route expiration-time"<<std::endl;
    std::cerr<<"    -mi <seconds> interval to run expired route management task, 5 by default."<<std::endl;
    std::cerr<<"    -mp <percent> maximum percent of expired routes removed at once."<<std::endl;
    std::cerr<<"    -mr <retries> maximum retries when trying to install new route"<<std::endl;
}

int main (int argc, char *argv[])
{
    //set timeouts used by background workers for network operations and some other events
    //increasing this time will slow down reaction to some internal and external events
    //decreasing this time too much will cause high cpu usage
    const int timeoutMs=500;
    const timeval timeoutTv={timeoutMs/1000,(timeoutMs-timeoutMs/1000*1000)*1000};

    //timeout for main thread waiting for external signals
    const timespec sigTs={2,0};

    StdioLoggerFactory logFactory;
    auto mainLogger=logFactory.CreateLogger("Main");

    //TODO: add sane option parsing
    if(argc<4)
    {
        usage(argv[0]);
        return 1;
    }

    //parse port
    if(std::strlen(argv[2])>5)
    {
        mainLogger->Error() << "port number is too long!" << std::endl;
        return 1;
    }

    //parse optional parameters
    const int metric=(argc>4)?std::atoi(argv[4]):100;
    const int ksMetric=(argc>5)?std::atoi(argv[5]):101;
    const bool useByteSwap=(argc>6)?(std::strncmp(argv[6],"true",4)==0):false;
    const IPAddress gateway4=(argc>7)?IPAddress(argv[7]):IPAddress("127.0.0.1");
    const IPAddress gateway6=(argc>8)?IPAddress(argv[8]):IPAddress("::1");
    const uint extraTTL=(argc>9)?(uint)std::atoi(argv[9]):(uint)(120*60);
    const int mgIntervalSec=(argc>10)?std::atoi(argv[10]):5;
    const int mgPercent=(argc>11)?std::atoi(argv[11]):5; //management worker run every 5 seconds (approximately)
    const int addRetryCnt=(argc>12)?std::atoi(argv[12]):60; //with 5 seconds interval it gives us 300 seconds

    bool gw4Set=false;
    bool gw6Set=false;

    if(metric<1)
    {
        mainLogger->Error() << "metric number is invalid!" << std::endl;
        return 1;
    }

    if(ksMetric<1||ksMetric<=metric)
    {
        mainLogger->Error() << "killswitch metric number is incorrect, it must be > main metric" << std::endl;
        return 1;
    }

    if(!gateway4.isValid||gateway4.isV6)
    {
        mainLogger->Error() << "ipv4 gateway address is invalid!" << std::endl;
        return 1;
    }

    if(!gateway4.Equals(IPAddress("127.0.0.1")))
        gw4Set=true;

    if(!gateway6.isValid||!gateway6.isV6)
    {
        mainLogger->Error() << "ipv6 gateway address is invalid!" << std::endl;
        return 1;
    }

    if(!gateway6.Equals(IPAddress("::1")))
        gw6Set=true;

    if(extraTTL<1)
    {
        mainLogger->Error() << "extraTTL time is invalid!" << std::endl;
        return 1;
    }

    if(mgIntervalSec<1)
    {
        mainLogger->Error() << "management interval time is invalid!" << std::endl;
        return 1;
    }

    if(mgPercent<1 || mgPercent>100)
    {
        mainLogger->Error() << "management percent value is invalid!" << std::endl;
        return 1;
    }

    if(addRetryCnt<1)
    {
        mainLogger->Error() << "route-add max retries count is invalid" << std::endl;
        return 1;
    }

    //loggers
    auto routingMgrLogger=logFactory.CreateLogger("Routing Manager");
    auto dnsReceiverLogger=logFactory.CreateLogger("DNS Receiver");
    auto trackerLogger=logFactory.CreateLogger("Network Tracker");

    //configure essential stuff
    MessageBroker messageBroker;
    ShutdownHandler shutdownHandler;
    messageBroker.AddSubscriber(shutdownHandler);

    //create main worker-instances
    RoutingManager routingMgr(*routingMgrLogger,argv[3],gw4Set?gateway4:IPAddress(),gw6Set?gateway6:IPAddress(),extraTTL,mgIntervalSec,mgPercent,metric,ksMetric,addRetryCnt);
    messageBroker.AddSubscriber(routingMgr);
    DNSReceiver dnsReceiver(*dnsReceiverLogger,messageBroker,timeoutTv,IPAddress(argv[1]),std::atoi(argv[2]),useByteSwap);
    NetDevTracker tracker(*trackerLogger,messageBroker,timeoutTv,metric,argv[3]);

    //create sigset_t struct with signals
    sigset_t sigset;
    sigemptyset(&sigset);
    if(sigaddset(&sigset,SIGHUP)!=0||sigaddset(&sigset,SIGTERM)!=0||sigaddset(&sigset,SIGUSR1)!=0||sigaddset(&sigset,SIGUSR2)!=0||pthread_sigmask(SIG_BLOCK,&sigset,nullptr)!=0)
    {
        mainLogger->Error()<<"Failed to setup signal-handling"<<std::endl;
        return 1;
    }

    //start background workers, or perform post-setup init
    routingMgr.Startup();
    dnsReceiver.Startup();
    tracker.Startup();

    while(true)
    {
        auto signal=sigtimedwait(&sigset,nullptr,&sigTs);
        auto error=errno;
        if(signal<0 && error!=EAGAIN && error!=EINTR)
        {
            mainLogger->Error()<<"Error while handling incoming signal: "<<strerror(error)<<std::endl;
            break;
        }
        else if(signal>0 && signal!=SIGUSR2 && signal!=SIGINT) //SIGUSR2 triggered by shutdownhandler to unblock sigtimedwait
        {
            mainLogger->Info()<< "Pending shutdown by receiving signal: "<<signal<<"->"<<strsignal(signal)<<std::endl;
            break;
        }

        if(shutdownHandler.IsShutdownRequested())
        {
            if(shutdownHandler.GetEC()!=0)
                mainLogger->Error() << "One of background worker was failed, shuting down" << std::endl;
            else
                mainLogger->Info() << "Shuting down gracefully by request from background worker" << std::endl;
            break;
        }
    }

    //request shutdown of background workers
    dnsReceiver.RequestShutdown();
    tracker.RequestShutdown();
    routingMgr.RequestShutdown();

    //wait for background workers shutdown complete
    dnsReceiver.Shutdown();
    tracker.Shutdown();
    routingMgr.Shutdown();

    logFactory.DestroyLogger(trackerLogger);
    logFactory.DestroyLogger(dnsReceiverLogger);
    logFactory.DestroyLogger(routingMgrLogger);
    logFactory.DestroyLogger(mainLogger);

    return  0;
}

