#ifndef IMESSAGE_H
#define IMESSAGE_H

enum MsgType
{
    MSG_SHUTDOWN,
    MSG_STATE_CHANGED,
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

#endif // IMESSAGE_H
