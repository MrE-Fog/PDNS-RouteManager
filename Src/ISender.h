#ifndef ISENDER_H
#define ISENDER_H

#include "IMessage.h"

class ISender
{
    public:
        virtual void SendMessage(const void * const sender, const IMessage &message) = 0;
};

#endif // ISENDER_H

