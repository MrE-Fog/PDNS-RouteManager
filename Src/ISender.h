#ifndef ICONTROL_H
#define ICONTROL_H

#include "IMessage.h"

class ISender
{
    public:
        virtual void SendMessage(const void * const sender, const IMessage &message) = 0;
};

#endif // ICONTROL_H

