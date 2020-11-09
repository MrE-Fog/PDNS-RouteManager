#ifndef ROUTINGMANAGER_H
#define ROUTINGMANAGER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "IMessageSubscriber.h"
#include "WorkerBase.h"

#include <mutex>
#include <atomic>

class RoutingManager : public IMessageSubscriber, public WorkerBase
{
    private:
        ILogger &logger;
        const char * const ifname;
        const IPAddress gateway;
        const uint extraTtl;
        const int mgIntervalSec;
        const int mgPercent;

        std::mutex opLock;
        std::atomic<bool> shutdownPending;
        int sock;
    public:
        RoutingManager(ILogger &logger, const char * const ifname, const IPAddress gateway, const uint extraTtl, const int mgIntervalSec, const int mgPercent);
        //WorkerBase
        void Worker() final;
        void OnShutdown() final;
        bool Startup() final;
        bool Shutdown() final;
        //IMessageSubscriber
        bool ReadyForMessage(const MsgType msgType) final;
        void OnMessage(const IMessage &message) final;
};

#endif // ROUTINGMANAGER_H
