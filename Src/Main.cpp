#include "ILogger.h"
#include "WorkerBase.h"
#include "IControl.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"

#include <iostream>
#include <atomic>
#include <thread>
#include <chrono>

//TODO: improve this
class ShutdownControl final: public IControl
{
    public:
        std::atomic<bool> shutdownRequested;
        std::atomic<int> ec;
        ShutdownControl() { shutdownRequested.store(false); ec.store(0); }
        void Shutdown(int _ec) final
        {
            if(!shutdownRequested.exchange(true))
                ec.store(_ec);
        }
};

int usage(ILogger &logger, const char * const self)
{
    logger.Error() << "Usage: " << self << " <listen ip-addr> <port> <target netdev>" << std::endl;
    return 1;
}

int main (int argc, char *argv[])
{
    const int timeoutMs=1000; //TODO: read timeouts from config
    const timeval timeoutTv = { timeoutMs/1000, (timeoutMs-timeoutMs/1000*1000)*1000 };

    StdioLogger logger;
    ShutdownControl control;

    if(argc!=4)
        return usage(logger,argv[0]);

    //create main worker-instances
    NetDevTracker tracker(logger,control,timeoutTv,argv[3]);

    //init
    tracker.Startup();

    //TODO: wait for termination signal(s), or call for shutdown
    int rounds=40; //for tests, remove
    while(true)
    {
        if(rounds<1)
            break;
        //TODO: use some sync-primitive instead of waiting;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        rounds--;
        if(control.shutdownRequested.load())
        {
            if(control.ec!=0)
                logger.Error() << "One of background worker was failed, shuting down" << std::endl;
            else
                logger.Info() << "Shuting down gracefully by signal from background worker" << std::endl;
            break;
        }
    }

    //shutdown
    tracker.Shutdown();

    return  0;
}
