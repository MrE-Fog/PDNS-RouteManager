#ifndef SHUTDOWNHANDLER_H
#define SHUTDOWNHANDLER_H

#include "ISubscriber.h"
#include <atomic>
#include <utility>

class ShutdownHandler final: public ISubscriber
{
    private:
        std::atomic<bool> shutdownRequested;
        std::atomic<int> ec;
    public:
        ShutdownHandler();
        bool IsShutdownRequested();
        int GetEC();
        //methods for ISubscriber
        bool ReadyForMessage(const MsgType msgType) final;
        void OnMessage(const IMessage &message) final;
};

#endif // SHUTDOWNHANDLER_H
