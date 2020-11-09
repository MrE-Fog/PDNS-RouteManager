#ifndef ROUTINGMANAGER_H
#define ROUTINGMANAGER_H

#include "IMessageSubscriber.h"
#include "WorkerBase.h"

#include <mutex>
#include <atomic>

//TODO: gateway, extra ttl, management interval,
class RoutingManager : public IMessageSubscriber, public WorkerBase
{
    private:
        std::mutex opLock;
        std::atomic<bool> shutdownPending;
        int socket;
    public:
        RoutingManager();
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
