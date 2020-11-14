#include "ILogger.h"
#include "StdioLoggerFactory.h"
#include "RoutingManager.h"
#include "NetDevTracker.h"
#include "DNSReceiver.h"
#include "StateSaver.h"
#include "MessageBroker.h"
#include "ShutdownHandler.h"

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <csignal>
#include <string>
#include <unordered_map>

void usage(const std::string &self)
{
    std::cerr<<"Usage: "<<self<<" [parameters]"<<std::endl;
    std::cerr<<"  mandatory parameters:"<<std::endl;
    std::cerr<<"    -l <ip-addr> listen ip-address to receive protobuf-encoded DNS packages."<<std::endl;
    std::cerr<<"    -p <port-num> TCP port number to listen at."<<std::endl;
    std::cerr<<"    -i <if-name> network interface that will be used for routing."<<std::endl;
    std::cerr<<"  optional parameters:"<<std::endl;
    std::cerr<<"    -rp <route priority> metric/priority number for generated routes."<<std::endl;
    std::cerr<<"     100 by default. MUST NOT INTERFERE WITH ANY OTHER SYSTEM ROUTES"<<std::endl;
    std::cerr<<"    -bp <blackhole-route priority> metric/priority number for generated"<<std::endl;
    std::cerr<<"     killswitch (blackhole) protective routes, rp+1 by default."<<std::endl;
    std::cerr<<"     MUST NOT INTERFERE WITH ANY OTHER ROUTES and must be higher than -rp"<<std::endl;
    std::cerr<<"    -s <true|false> swap bytes when decoding protobuf messages."<<std::endl;
    std::cerr<<"     true if PDNS service is running on architecture with different endianness"<<std::endl;
    std::cerr<<"    -gw4 <ip-addr> ipv4 gateway address. not used with p-t-p interfaces"<<std::endl;
    std::cerr<<"    -gw6 <ip-addr> ipv6 gateway address. not used with p-t-p interfaces"<<std::endl;
    std::cerr<<"    -ttl <seconds> additional time interval added to route expiration-time."<<std::endl;
    std::cerr<<"    -mi <seconds> interval to run expired route management task, 5 by default."<<std::endl;
    std::cerr<<"    -mp <percent> maximum percent of expired routes removed at once."<<std::endl;
    std::cerr<<"    -mr <retries> maximum retries when trying to install new route"<<std::endl;
    std::cerr<<"  TODO: save and restore active routes to backup file for crash recovery"<<std::endl;
    std::cerr<<"    -fr <filename> file with backup of current routes, used for crash recover"<<std::endl;
    std::cerr<<"    -fi <seconds> approximate interval between attempting to perform save"<<std::endl;
}

int param_error(const std::string &self, const std::string &message)
{
    std::cerr<<message<<std::endl;
    usage(self);
    return 1;
}

int main (int argc, char *argv[])
{
    //set timeouts used by background workers for network operations and some other events
    //increasing this time will slow down reaction to some internal and external events
    //decreasing this time too much will cause high cpu usage
    const int timeoutMs=500;
    const struct timeval timeoutTv={timeoutMs/1000,(timeoutMs-timeoutMs/1000*1000)*1000};

    //timeout for main thread waiting for external signals
    const timespec sigTs={2,0};

    std::unordered_map<std::string,std::string> args;
    bool isArgValue=false;
    for(auto i=1;i<argc;++i)
    {
        if(isArgValue)
        {
            args[argv[i-1]]=argv[i];
            isArgValue=false;
            continue;
        }
        if(std::string(argv[i]).length()<2||std::string(argv[i]).front()!='-')
        {
            std::cerr<<"Invalid cmdline argument: "<<argv[i]<<std::endl;
            usage(argv[0]);
            return 1;
        }
        isArgValue=true;
    }

    if(args.empty())
        return param_error(argv[0],"Mandatory parameters are missing!");

    //parse listen address
    if(args.find("-l")==args.end())
        return param_error(argv[0],"Listen address is missing!");
    IPAddress listenAddr(args["-l"]);
    if(!listenAddr.isValid)
        return param_error(argv[0],"Listen address is invalid!");

    //parse port number
    if(args.find("-p")==args.end())
        return param_error(argv[0],"TCP port number is missing!");
    //parse port
    if(args["-p"].length()>5||args["-p"].length()<1)
        return param_error(argv[0],"TCP port number is too long or invalid!");
    auto port=std::atoi(args["-p"].c_str());
    if(port<1||port>65535)
        return param_error(argv[0],"TCP port number is invalid!");

    //parse interface name
    if(args.find("-i")==args.end()||args["-i"].length()<1)
        return param_error(argv[0],"Target interface name is missing or invalid!");

    //optional params

    //route priority
    int metric=100;
    if(args.find("-rp")!=args.end())
    {
        metric=std::atoi(args["-rp"].c_str());
        if(metric<1)
            return param_error(argv[0],"Route priority value is invalid!");
    }

    //blackhole/killswitch route priority
    int ksMetric=metric+1;
    if(args.find("-bp")!=args.end())
    {
        ksMetric=std::atoi(args["-bp"].c_str());
        if(ksMetric<1)
            return param_error(argv[0],"Blackhole route priority value is invalid!");
        if(ksMetric<=metric)
            return param_error(argv[0],"Blackhole route priority value must be > than regular route priority");
    }

    //byte swap
    bool useByteSwap=false;
    if(args.find("-s")!=args.end())
    {
        if(args["-s"]=="true")
            useByteSwap=true;
        else if(args["-s"]=="false")
            useByteSwap=true;
        else
            return param_error(argv[0],"Use swap parameter is invalid!");
    }

    //gateway4
    bool gw4Set=false;
    if(args.find("-gw4")!=args.end())
    {
        if(!IPAddress(args["-gw4"]).isValid||IPAddress(args["-gw4"]).isV6)
            return param_error(argv[0],"IPv4 gateway is invalid!");
        gw4Set=true;
    }
    const IPAddress gateway4=gw4Set?IPAddress(args["-gw4"]):IPAddress();

    //gateway6
    bool gw6Set=false;
    if(args.find("-gw6")!=args.end())
    {
        if(!IPAddress(args["-gw6"]).isValid||!IPAddress(args["-gw6"]).isV6)
            return param_error(argv[0],"IPv6 gateway is invalid!");
        gw6Set=true;
    }
    const IPAddress gateway6=gw6Set?IPAddress(args["-gw6"]):IPAddress();

    //route priority
    int extraTTL=60*150; //150 minutes - 2.5 houres
    if(args.find("-ttl")!=args.end())
    {
        extraTTL=std::atoi(args["-ttl"].c_str());
        if(extraTTL<1)
            return param_error(argv[0],"Extra protective TTL value is invalid!");
    }

    //management interval
    int mgIntervalSec=5;
    if(args.find("-mi")!=args.end())
    {
        mgIntervalSec=std::atoi(args["-mi"].c_str());
        if(mgIntervalSec<1)
            return param_error(argv[0],"Management interval time is invalid!");
    }

    //management percent
    int mgPercent=5;
    if(args.find("-mp")!=args.end())
    {
        mgPercent=std::atoi(args["-mp"].c_str());
        if(mgPercent<1||mgPercent>100)
            return param_error(argv[0],"Percent of routes managed at once is invalid!");
    }

    //retry count
    int addRetryCnt=60;
    if(args.find("-mr")!=args.end())
    {
        addRetryCnt=std::atoi(args["-mr"].c_str());
        if(addRetryCnt<1)
            return param_error(argv[0],"Route-add retry count is invalid");
    }

    std::string saveFile=args.find("-fr")!=args.end()?args["-fr"]:"";

    int saveInterval=5;
    if(args.find("-fi")!=args.end())
    {
        saveInterval=std::atoi(args["-fi"].c_str());
        if(saveInterval<1)
            return param_error(argv[0],"Backup file save interval is incorrect");
    }

    StdioLoggerFactory logFactory;
    auto mainLogger=logFactory.CreateLogger("Main");
    auto routingMgrLogger=logFactory.CreateLogger("RT_Man");
    auto dnsReceiverLogger=logFactory.CreateLogger("DNS_Rc");
    auto trackerLogger=logFactory.CreateLogger("ND_Trk");
    auto saverLogger=logFactory.CreateLogger("ST_Svr");


    //dump current configuration
    mainLogger->Info()<<"Starting up";
    mainLogger->Info()<<"listening at "<<listenAddr<<" port "<<port<<"; routing via "<<args["-i"]<<" interface";
    mainLogger->Info()<<"route prio: "<<metric<<"; blkhole-route prio: "<<ksMetric<<"; extra ttl: "<<extraTTL<<"; use byte swap: "<<(useByteSwap?"true":"false");
    mainLogger->Info()<<"ipv4 gateway: "<<(gw4Set?gateway4.ToString():std::string("not set"))<<"; ipv6 gateway: "<<(gw6Set?gateway6.ToString():std::string("not set"));
    mainLogger->Info()<<"management interval: "<<mgIntervalSec<<"; percent of routes to manage at once: "<<mgPercent<<"%; route-add max tries count: "<<addRetryCnt;

    //configure essential stuff
    MessageBroker messageBroker;
    ShutdownHandler shutdownHandler;
    messageBroker.AddSubscriber(shutdownHandler);

    //create main worker-instances
    RoutingManager routingMgr(*routingMgrLogger,args["-i"],gateway4,gateway6,extraTTL,mgIntervalSec,mgPercent,metric,ksMetric,addRetryCnt);
    messageBroker.AddSubscriber(routingMgr);
    DNSReceiver dnsReceiver(*dnsReceiverLogger,messageBroker,timeoutTv,listenAddr,port,useByteSwap);
    NetDevTracker tracker(*trackerLogger,messageBroker,args["-i"],timeoutTv,metric);
    StateSaver saver(*saverLogger, saveFile, saveInterval, timeoutMs);
    if(!saveFile.empty())
        messageBroker.AddSubscriber(saver);

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
    if(!saveFile.empty())
        saver.Startup();

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
    if(!saveFile.empty())
        saver.RequestShutdown();

    //wait for background workers shutdown complete
    dnsReceiver.Shutdown();
    tracker.Shutdown();
    routingMgr.Shutdown();
    if(!saveFile.empty())
        saver.Shutdown();

    logFactory.DestroyLogger(saverLogger);
    logFactory.DestroyLogger(trackerLogger);
    logFactory.DestroyLogger(dnsReceiverLogger);
    logFactory.DestroyLogger(routingMgrLogger);
    logFactory.DestroyLogger(mainLogger);

    return  0;
}

