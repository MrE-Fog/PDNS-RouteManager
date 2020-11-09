#ifndef NETDEVTRACKER_H
#define NETDEVTRACKER_H

#include "IWorker.h"
#include "ILogger.h"

class NetDevTracker final : public IWorker
{
    private:
        const ILogger &logger;
    public:
        NetDevTracker(const ILogger &logger);
        void Startup() final;
        void Shutdown() final;
};

#endif // NETDEVTRACKER_H
