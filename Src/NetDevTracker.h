#ifndef NETDEVTRACKER_H
#define NETDEVTRACKER_H

#include "WorkerBase.h"
#include "ILogger.h"
#include "IControl.h"
#include <atomic>

class NetDevTracker final : public WorkerBase
{
    private:
        const char * const ifname;
        const timeval &timeout;
        ILogger& logger;
        std::atomic<bool> shutdownRequested;

        void Worker() final;
        void RequestShutdown() final;
    public:
        NetDevTracker(ILogger &logger, IControl &control, const timeval &timeout, const char * const ifname);
};

#endif // NETDEVTRACKER_H
