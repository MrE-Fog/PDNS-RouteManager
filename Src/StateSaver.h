#ifndef STATESAVER_H
#define STATESAVER_H

#include "ILogger.h"
#include "IMessageSubscriber.h"
#include "WorkerBase.h"
#include "IPAddress.h"

#include <atomic>
#include <mutex>
#include <unordered_map>
#include <queue>

//TODO: not complete
class StateSaver : public IMessageSubscriber, public WorkerBase
{
    private:
        ILogger &logger;
        const std::string filename;
        const int saveInterval;
        const int sleepMS;
        std::atomic<bool> shutdownRequested;
        std::mutex opLock;
        int state;
        std::unordered_map<IPAddress,std::pair<uint64_t,bool>> routes;
        void SaveRoutes(int routeSaveCount);
    public:
        StateSaver(ILogger &logger, const std::string &filename, const int saveInterval, const int sleepMS);
        //WorkerBase
        void Worker() final;
        void OnShutdown() final;
        //IMessageSubscriber
        bool ReadyForMessage(const MsgType msgType) final;
        void OnMessage(const IMessage &message) final;
};

#endif // STATESAVER_H
