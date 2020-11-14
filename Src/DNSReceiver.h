#ifndef DNSRECEIVER_H
#define DNSRECEIVER_H

#include "ILogger.h"
#include "IPAddress.h"
#include "WorkerBase.h"
#include "IMessageSender.h"

#include <atomic>
#include <sys/time.h>

class DNSReceiver : public WorkerBase
{
    private:
        ILogger &logger;
        IMessageSender &sender;
        const timeval timeout;
        const IPAddress listenAddr;
        const int port;
        const bool useByteSwap;
        std::atomic<bool> shutdownPending;

        void HandleError(int ec, const std::string& message);
        void HandleError(const std::string &message);
        uint16_t DecodeHeader(const void * const data) const;
    public:
        DNSReceiver(ILogger &logger, IMessageSender &sender, const timeval timeout, const IPAddress listenAddr, const int port, const bool useByteSwap);
    protected: //WorkerBase
        void Worker() final;
        void OnShutdown() final;
};

#endif // DNSRECEIVER_H
