#include "DNSReceiver.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

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

    auto dTimeout=std::chrono::seconds(timeout.tv_sec)+std::chrono::microseconds(timeout.tv_usec);

    while(!shutdownPending.load())
    {
        //create listen socket
        auto lSockFd=socket(listenAddr.isV6?AF_INET6:AF_INET,SOCK_STREAM,0);
        if(lSockFd==-1)
        {
            HandleError(errno,"Failed to create listen socket: ");
            return;
        }

        //tune some some options
        int sockReuseAddrEnabled=1;
        if (setsockopt(lSockFd, SOL_SOCKET, SO_REUSEADDR, &sockReuseAddrEnabled, sizeof(int))!=0)
        {
            HandleError(errno,"Failed to set SO_REUSEADDR option: ");
            return;
        }
#ifdef SO_REUSEPORT
        int sockReusePortEnabled=1;
        if (setsockopt(lSockFd, SOL_SOCKET, SO_REUSEPORT, &sockReusePortEnabled, sizeof(int))!=0)
        {
            HandleError(errno,"Failed to set SO_REUSEPORT option: ");
            return;
        }
#endif

        linger lLinger={1,0};
        if (setsockopt(lSockFd, SOL_SOCKET, SO_LINGER, &lLinger, sizeof(linger))!=0)
        {
            HandleError(errno,"Failed to set SO_LINGER option: ");
            return;
        }

        bool bindFailWarned=false;
        bool bindComplete=false;
        while(!bindComplete && !shutdownPending.load())
        {
            //check for shutdown ?

            sockaddr_in ipv4Addr = {};
            sockaddr_in6 ipv6Addr = {};
            sockaddr *target;
            socklen_t len;
            if(listenAddr.isV6)
            {
                ipv6Addr.sin6_family=AF_INET6;
                ipv6Addr.sin6_port=htons((unsigned short)port);
                listenAddr.ToSA(&ipv6Addr);
                target=(sockaddr*)&ipv6Addr;
                len=sizeof(sockaddr_in6);
            }
            else
            {
                ipv4Addr.sin_family=AF_INET;
                ipv4Addr.sin_port=htons((unsigned short)port);
                listenAddr.ToSA(&ipv4Addr);
                target=(sockaddr*)&ipv4Addr;
                len=sizeof(sockaddr_in);
            }

            if (bind(lSockFd,target,len)!=0)
            {
                if(!bindFailWarned)
                {
                    bindFailWarned=true;
                    logger.Warning()<<"Failed to bind listen socket: "<<strerror(errno)<<std::endl;
                }
                std::this_thread::sleep_for(dTimeout);
                continue;
            }
            if (listen(lSockFd,1)!=0)
            {
                HandleError(errno,"Failed to setup listen socket: ");
                return;
            }
            logger.Info()<<"Listening for incoming connection"<<std::endl;
            bindComplete=true;

            bool connAccepted=false;
            while (!connAccepted && !shutdownPending.load())
            {
                //wait for new data ready to be read from netlink
                fd_set set;
                FD_ZERO(&set);
                FD_SET(lSockFd, &set);
                auto t = timeout;
                auto rv = select(lSockFd+1, &set, NULL, NULL, &t);
                if(rv==0) //no incoming connection detected
                    continue;
                if(rv<0)
                {
                    auto error=errno;
                    if(error==EINTR)//interrupted by signal
                        break;
                    HandleError(error,"Error awaiting incoming connection: ");
                    return;
                }

                //accept single connection
                sockaddr_storage cAddr;
                socklen_t cAddrSz = sizeof(cAddr);
                auto cSockFd=accept(lSockFd,(sockaddr*)&cAddr,&cAddrSz);
                if(cSockFd<1)
                {
                    logger.Warning()<<"Failed to accept connection: "<<strerror(errno)<<std::endl;
                    continue;
                }
                linger cLinger={1,0};
                if (setsockopt(cSockFd, SOL_SOCKET, SO_LINGER, &cLinger, sizeof(linger))!=0)
                    logger.Warning()<<"Failed to set SO_LINGER option to client socket: "<<strerror(errno)<<std::endl;
                connAccepted=true;

                logger.Info()<<"Client connected"<<std::endl;
                //receive dnsdist packages until receive socket is receiving
                bool recvComplete=false;
                while(!recvComplete && !shutdownPending.load())
                {
                    //check for shutdown is pending
                    //if no new data can be read, then recvComplete=true
                    recvComplete=true;
                }

                logger.Info()<<"Closing client connection"<<std::endl;
                if(close(cSockFd)!=0)
                {
                    HandleError(errno,"Failed to close client socket: ");
                    return;
                }
            }
        }
        if(close(lSockFd)!=0)
        {
            HandleError(errno,"Failed to close listen socket: ");
            return;
        }
    }

    logger.Info()<<"Shuting down DNSReceiver worker thread"<<std::endl;
}
