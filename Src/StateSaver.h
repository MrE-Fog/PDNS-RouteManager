#ifndef STATESAVER_H
#define STATESAVER_H

#include "ILogger.h"
#include "IMessageSubscriber.h"
#include "WorkerBase.h"

#include <atomic>
#include <mutex>

class StateSaver : public IMessageSubscriber, public WorkerBase
{
    private:
        ILogger &logger;
        std::atomic<bool> shutdownRequested;
        std::mutex opLock;

    public:
        StateSaver(ILogger &logger);
        //WorkerBase
        void Worker() final;
        void OnShutdown() final;
        //IMessageSubscriber
        bool ReadyForMessage(const MsgType msgType) final;
        void OnMessage(const IMessage &message) final;
};

#endif // STATESAVER_H
