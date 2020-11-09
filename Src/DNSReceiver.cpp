#include "DNSReceiver.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };

DNSReceiver::DNSReceiver(ILogger &_logger, IMessageSender &_sender, const timeval &_timeout, const IPAddress _listenAddr, const int _port, const bool useByteSwap):
    logger(_logger),
    sender(_sender),
    timeout(_timeout),
    listenAddr(_listenAddr),
    port(_port),
    pbHelper(ProtobufHelper(_logger,useByteSwap))
{
    shutdownPending.store(false);
}

void DNSReceiver::HandleError(const char * const message)
{
    logger.Error()<<message<<std::endl;
    sender.SendMessage(this,ShutdownMessage(1));
}

void DNSReceiver::HandleError(int ec, const char * const message)
{
    logger.Error()<<message<<strerror(ec)<<std::endl;
    sender.SendMessage(this,ShutdownMessage(ec));
}

void DNSReceiver::OnShutdown()
{
    shutdownPending.store(true);
}

void DNSReceiver::Worker()
{
    if(!listenAddr.isValid)
    {
        HandleError("Listen IP address is invalid");
        return;
    }

    if(port<1||port>65535)
    {
        HandleError("Port number is invalid");
        return;
    }

    while(!shutdownPending.load())
    {
        //create listen socket
        auto lSockFd=socket(listenAddr.isV6?AF_INET6:AF_INET,SOCK_STREAM,0);
        if(lSockFd==-1)
        {
            HandleError(errno,"Failed to create listen socket: ");
            return;
        }

        bool bindComplete=false;
        while(!bindComplete )
        {
            //check for shutdown
            sockaddr_in ipv4Addr = {};
            sockaddr_in6 ipv6Addr = {};
            sockaddr *target;
            socklen_t len;
            if(listenAddr.isV6)
            {
                ipv6Addr.sin6_family=AF_INET6;
                ipv6Addr.sin6_port=(unsigned short)port;
                listenAddr.ToSA(&ipv6Addr);
                target=(sockaddr*)&ipv6Addr;
                len=sizeof(sockaddr_in6);
            }
            else
            {
                ipv4Addr.sin_family=AF_INET;
                ipv4Addr.sin_port=(unsigned short)port;
                listenAddr.ToSA(&ipv4Addr);
                target=(sockaddr*)&ipv4Addr;
                len=sizeof(sockaddr_in);
            }

            // Binding newly created socket to given IP and verification
            if (bind(lSockFd,target,len)!=0)
            {
                //wait for some time, retry
                continue;
            }
            bindComplete=true;
            //accept connection, escape this loop if failed (+wait)

            //receive dnsdist packages until receive socket is receiving
            bool recvComplete=false;
            while(!recvComplete)
            {
                //check for shutdown is pending
            }

            //close receive socket
        }
        //close listen socket
    }
}
