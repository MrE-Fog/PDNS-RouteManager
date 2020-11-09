#ifndef DNSRECEIVER_H
#define DNSRECEIVER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "WorkerBase.h"
#include "IMessageSender.h"
#include "ProtobufHelper.h"

#include <atomic>

class DNSReceiver : public WorkerBase
{
    private:
        ILogger &logger;
        IMessageSender &sender;
        const timeval &timeout;
        const IPAddress listenAddr;
        const int port;
        const ProtobufHelper pbHelper;
        std::atomic<bool> shutdownPending;

        void HandleError(int ec, const char* const message);
        void HandleError(const char* const message);
    public:
        DNSReceiver(ILogger &logger, IMessageSender &sender, const timeval &timeout, const IPAddress listenAddr, const int port, const bool useByteSwap);
    //WorkerBase
    protected:
        void Worker() final;
        void OnShutdown() final;
};

#endif // DNSRECEIVER_H
