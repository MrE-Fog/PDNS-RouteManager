#include "DNSReceiver.h"
#include "dnsmessage.pb.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <string>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };
class RouteRequestMessage: public IRouteRequestMessage { public: RouteRequestMessage(const IPAddress &_ip, const uint _ttl):IRouteRequestMessage(_ip,_ttl){} };

DNSReceiver::DNSReceiver(ILogger &_logger, IMessageSender &_sender, const timeval _timeout, const IPAddress _listenAddr, const int _port, const bool _useByteSwap):
    logger(_logger),
    sender(_sender),
    timeout(_timeout),
    listenAddr(_listenAddr),
    port(_port),
    useByteSwap(_useByteSwap)
{
    shutdownPending.store(false);
}

uint16_t DNSReceiver::DecodeHeader(const void * const data) const
{
    return useByteSwap?(uint16_t)((*((const uint16_t*)data)&0x00FF)<<8|(*((const uint16_t*)data)&0xFF00)>>8):*((const uint16_t*)data);
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
                //wait for new connection
                fd_set lSet;
                FD_ZERO(&lSet);
                FD_SET(lSockFd, &lSet);
                auto lt = timeout;
                auto lrv = select(lSockFd+1, &lSet, NULL, NULL, &lt);
                if(lrv==0) //no incoming connection detected
                    continue;
                if(lrv<0)
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
                bool headerPending=true;
                size_t dataLeft=2;
                size_t dataSize=2;
                unsigned char* data[65536]={}; //uint16_t header may only encode 64kib of data
                while(!shutdownPending.load())
                {
                    //wait for data
                    fd_set cSet;
                    FD_ZERO(&cSet);
                    FD_SET(cSockFd, &cSet);
                    auto ct = timeout;
                    auto crv = select(cSockFd+1, &cSet, NULL, NULL, &ct);
                    if(crv==0) //no incoming connection detected
                        continue;
                    if(crv<0)
                    {
                        auto error=errno;
                        if(error==EINTR)//interrupted by signal
                            break;
                        HandleError(error,"Error awaiting data from client: ");
                        return;
                    }
                    //read data
                    auto dataRead=read(cSockFd,(void*)(data+dataSize-dataLeft),dataLeft);
                    if(dataRead==0)
                    {
                        logger.Info()<<"Client disconnected"<<std::endl;
                        break; //connection closed
                    }
                    if(dataRead<0)
                    {
                        logger.Warning()<<"Error reading data from client: "<<strerror(errno)<<std::endl;
                        break;
                    }
                    dataLeft-=dataRead;
                    if(dataLeft>0) //we still need to read more data
                        continue;
                    if(headerPending)
                    {//decore header, setup read of data-payload
                        dataSize=DecodeHeader(data);
                        if(dataSize<1)
                            dataSize=2; //zero sized payload, setup read for another header
                        else
                            headerPending=false;
                        dataLeft=dataSize;
                    }
                    else
                    {//decode payload, setup read of next data-header
                        //decode payload
                        PBDNSMessage message;
                        if(!message.ParseFromArray(data,(int)dataSize))
                            logger.Warning()<<"Failed to decode payload of size "<<dataSize<<std::endl;
                        else if(!message.has_response() || message.response().rrs_size()<1)
                            logger.Warning()<<"No valid response or dns resource records provided in dnsdist message"<<std::endl;
                        else
                            //parse dns resource records
                            for(auto rIdx=0;rIdx<message.response().rrs_size();++rIdx)
                            {
                                auto record=message.response().rrs(rIdx);
                                std::string name=record.has_name()?record.name():"<NO NAME>";
                                auto type=record.has_type()?record.type():0;
                                auto ttl=record.has_ttl()?record.ttl():0;
                                auto rdata=record.has_rdata()?record.rdata():std::string();
                                if(rdata.empty()||(type!=1&&type!=28))
                                    logger.Warning()<<"Unsupported dns resource record provided -> name="<<name<<",type="<<type<<",ttl="<<ttl<<",rdata len="<<rdata.length()<<std::endl;
                                else
                                {
                                    IPAddress ip(rdata);
                                    if(!ip.isValid)
                                        logger.Warning()<<"Invalid ip address decoded for response -> name="<<name<<",type="<<type<<",ttl="<<ttl<<",rdata len="<<rdata.length()<<std::endl;
                                    else
                                    {
                                        logger.Info()<<"Valid response decoded -> name="<<name<<",ip="<<ip<<",type="<<type<<",ttl="<<ttl<<std::endl;
                                        sender.SendMessage(this,RouteRequestMessage(ip,ttl));
                                    }
                                }
                            }

                        //setup reading of new package
                        headerPending=true;
                        dataSize=dataLeft=2;
                    }
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
