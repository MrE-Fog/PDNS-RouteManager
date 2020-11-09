#ifndef ISUBSCRIBER_H
#define ISUBSCRIBER_H

#include "IMessage.h"

class ISubscriber
{
    public:
        virtual bool ReadyForMessage(const MsgType msgType) = 0;
        virtual void OnMessage(const IMessage &message) = 0;
};

#endif // ISUBSCRIBER_H
