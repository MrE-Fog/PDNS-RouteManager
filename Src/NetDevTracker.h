#ifndef NETDEVTRACKER_H
#define NETDEVTRACKER_H

#include "WorkerBase.h"
#include "ILogger.h"
#include "IControl.h"

class NetDevTracker final : public WorkerBase
{
    private:
        ILogger& logger;
        void Worker() final;
        void RequestShutdown() final;
    public:
        NetDevTracker(ILogger &logger, IControl &control);
};

#endif // NETDEVTRACKER_H
