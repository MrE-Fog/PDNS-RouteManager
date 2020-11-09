#ifndef ROUTINGMANAGER_H
#define ROUTINGMANAGER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "IMessageSubscriber.h"
#include "WorkerBase.h"

#include <mutex>
#include <atomic>
#include <ctime>

class RoutingManager : public IMessageSubscriber, public WorkerBase
{
    private:
        //constants and thread-safe stuff
        ILogger &logger;
        const char * const ifname;
        const IPAddress gateway;
        const uint extraTtl;
        const int mgIntervalSec;
        const int mgPercent;
        const int metric;
        const int ksMetric;
        //varous locking stuff and cross-thread counters
        std::mutex opLock;
        std::atomic<bool> shutdownPending;
        std::atomic<uint64_t> curTime;
        //all other fields must be accesed only using opLock mutex
        int sock;
        uint32_t seqNum;
        //unsorted set with non-confirmed and failed routes, must be reseeded as fast as possible
        //sorted set with active routes sorted by EOL time in accending order

        //internal service methods
        void CleanStaleRoutes();
        uint64_t UpdateCurTime();
    public:
        RoutingManager(ILogger &logger, const char * const ifname, const IPAddress gateway, const uint extraTtl, const int mgIntervalSec, const int mgPercent, const int metric, const int ksMetric);
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
