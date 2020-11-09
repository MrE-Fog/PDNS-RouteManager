#ifndef NETDEVTRACKER_H
#define NETDEVTRACKER_H

#include "WorkerBase.h"
#include "ILogger.h"
#include "IMessageSender.h"

#include <atomic>

class NetDevTracker final : public WorkerBase
{
    private:
        const char * const ifname;
        const timeval &timeout;
        ILogger &logger;
        IMessageSender &sender;
        std::atomic<bool> shutdownRequested;

        void HandleError(int ec, const char* message);
        //methods for WorkerBase
        void Worker() final;
        void OnShutdown() final;
    public:
        NetDevTracker(ILogger &logger, IMessageSender &sender, const timeval &timeout, const char * const ifname);
};

#endif // NETDEVTRACKER_H
