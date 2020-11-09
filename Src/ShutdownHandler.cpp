#include "ShutdownHandler.h"

ShutdownHandler::ShutdownHandler()
{
    shutdownRequested.store(false);
    ec.store(0);
}

bool ShutdownHandler::IsShutdownRequested()
{
    return shutdownRequested.load();
}

int ShutdownHandler::GetEC()
{
    return ec.load();
}

bool ShutdownHandler::ReadyForMessage(const MsgType msgType)
{
    return msgType==MSG_SHUTDOWN;
}

void ShutdownHandler::OnMessage(const IMessage& message)
{
    if(!shutdownRequested.exchange(true))
        ec.store(static_cast<const IShutdownMessage&>(message).ec);
}
