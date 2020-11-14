#ifndef IMESSAGE_H
#define IMESSAGE_H

#include "InterfaceConfig.h"
#include <vector>

enum MsgType
{
    MSG_SHUTDOWN,
    MSG_NETDEV_UPDATE,
    MSG_ROUTE_REQUEST,
    MSG_ROUTE_ADDED,
    MSG_ROUTE_REMOVED,
    MSG_SAVE_ROUTE,
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

class IRouteRequestMessage : public IMessage
{
    protected:
        IRouteRequestMessage(const IPAddress &_ip, const uint _ttl):IMessage(MSG_ROUTE_REQUEST),ip(_ip),ttl(_ttl){}
    public:
        const IPAddress &ip;
        const uint ttl;
};

class IRouteAddedMessage : public IMessage
{
    protected:
        IRouteAddedMessage(const IPAddress &_ip):IMessage(MSG_ROUTE_ADDED),ip(_ip){}
    public:
        const IPAddress &ip;
};

class IRouteRemovedMessage : public IMessage
{
    protected:
        IRouteRemovedMessage(const IPAddress &_ip):IMessage(MSG_ROUTE_REMOVED),ip(_ip){}
    public:
        const IPAddress &ip;
};

#endif // IMESSAGE_H
