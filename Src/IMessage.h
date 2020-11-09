#ifndef IMESSAGE_H
#define IMESSAGE_H

#include "InterfaceConfig.h"

enum MsgType
{
    MSG_SHUTDOWN,
    MSG_NETDEV_UPDATE,
};

class IMessage
{
    protected:
        IMessage(const MsgType _msgType):msgType(_msgType){};
    public:
        const MsgType msgType;
};

class IShutdownMessage : public IMessage
{
    protected:
        IShutdownMessage(int _ec):IMessage(MSG_SHUTDOWN),ec(_ec){}
    public:
        const int ec;
};

class INetDevUpdateMessage : public IMessage
{
    protected:
        INetDevUpdateMessage(InterfaceConfig &_config):IMessage(MSG_NETDEV_UPDATE),config(_config){}
    public:
        const InterfaceConfig config;
};

#endif // IMESSAGE_H
