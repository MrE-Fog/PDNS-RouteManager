#include "DNSReceiver.h"


DNSReceiver::DNSReceiver(ILogger &_logger, const IPAddress _listenAddr, const int _port, const bool useByteSwap):
    logger(_logger),
    listenAddr(_listenAddr),
    port(_port),
    pbHelper(ProtobufHelper(_logger,useByteSwap))
{

}

void DNSReceiver::Worker()
{
    logger.Info()<<"DNSReceiver starting at "<<listenAddr<<":"<<port<<std::endl;
}

void DNSReceiver::OnShutdown()
{

}
