#ifndef ROUTINGMANAGER_H
#define ROUTINGMANAGER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "InterfaceConfig.h"
#include "ImmutableStorage.h"
#include "Route.h"
#include "IMessageSubscriber.h"
#include "WorkerBase.h"

#include <mutex>
#include <atomic>
#include <ctime>
#include <unordered_map>
#include <set>
#include <unordered_set>

class RoutingManager : public IMessageSubscriber, public WorkerBase
{
    private:
        //constants and thread-safe stuff
        ILogger &logger;
        const char * const ifname;
        const IPAddress gateway4;
        const IPAddress gateway6;
        const uint extraTTL;
        const int mgIntervalSec;
        const int mgPercent;
        const int metric; //must be int, according to rtnetlink.7
        const int ksMetric; //must be int, according to rtnetlink.7
        //varous locking stuff and cross-thread counters
        std::mutex opLock;
        std::atomic<bool> shutdownPending;
        std::atomic<uint64_t> curTime;
        //all other fields must be accesed only using opLock mutex
        int sock;
        uint32_t seqNum;
        std::unordered_map<uint32_t,Route> pendingInserts; //non-confirmed and failed routes, must be reseeded as fast as possible, KEY is seqNum
        std::set<Route> activeRoutes; //currently active routes, sorted by expiration times
        std::unordered_set<IPAddress> pendingRemoves; //routes pending to remove, identified by IP
        ImmutableStorage<InterfaceConfig> ifCfg;
        //service methods that will use opLock internally
        void CleanStaleRoutes();
        void InsertRoute(const IPAddress &dest, uint ttl);
        void ProcessNetDevUpdate(const InterfaceConfig &newConfig);
        //internal service methods that is not using opLock.
        uint64_t _UpdateCurTime();
        uint32_t _UpdateSeqNum();
        void _ProcessPendingInserts();
        void _PushRoute(const Route &route);
    public:
        RoutingManager(ILogger &logger, const char * const ifname, const IPAddress &gateway4, const IPAddress &gateway6, const uint extraTTL, const int mgIntervalSec, const int mgPercent, const int metric, const int ksMetric);
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
