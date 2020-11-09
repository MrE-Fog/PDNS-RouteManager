#ifndef MESSAGEBROKER_H
#define MESSAGEBROKER_H

#include "ISender.h"
#include "ISubscriber.h"
#include <thread>
#include <mutex>
#include <map>
#include <set>

class MessageBroker : public ISender
{
    private:
        std::mutex opLock;
        std::set<ISubscriber*> subscribers;
        std::map<std::thread::id,std::set<const void*>*> callers;
    public:
        void AddSubscriber(ISubscriber& subscriber);
        void SendMessage(const void * const sender, const IMessage &message) final;
};

#endif // MESSAGEBROKER_H
