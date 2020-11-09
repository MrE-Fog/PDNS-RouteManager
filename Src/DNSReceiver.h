#ifndef DNSRECEIVER_H
#define DNSRECEIVER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "WorkerBase.h"
#include "IMessageSender.h"
#include "ProtobufHelper.h"

class DNSReceiver : public WorkerBase
{
    private:
        ILogger &logger;
        const IPAddress listenAddr;
        const int port;
        const ProtobufHelper pbHelper;
    public:
        DNSReceiver(ILogger &logger, const IPAddress listenAddr, const int port, const bool useByteSwap);
    //worker base
    protected:
        void Worker() final;
        void OnShutdown() final;
};

#endif // DNSRECEIVER_H
