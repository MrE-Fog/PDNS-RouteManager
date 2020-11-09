#include "NetDevTracker.h"
#include "IPAddress.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>

#include <unistd.h>
//#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };

NetDevTracker::NetDevTracker(ILogger &_logger, ISender &_sender, const timeval &_timeout, const char* const _ifname):
    ifname(_ifname),
    timeout(_timeout),
    logger(_logger),
    sender(_sender)
{
    shutdownRequested.store(false);
}

void NetDevTracker::OnShutdown()
{
    shutdownRequested.store(true);
}

void NetDevTracker::HandleError(int ec, const char* message)
{
    logger.Error()<<message<<strerror(ec)<<std::endl;
    sender.SendMessage(this,ShutdownMessage(ec));
}

void NetDevTracker::Worker()
{
    logger.Info()<<"Tracking network interface: "<<ifname<<std::endl;

    auto sock=socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(sock==-1)
    {
        HandleError(errno,"Failed to open netlink socket: ");
        return;
    }

    sockaddr_nl nlAddr = {};
    nlAddr.nl_family=AF_NETLINK;
    nlAddr.nl_groups=RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

    if (bind(sock, (sockaddr *)&nlAddr, sizeof(nlAddr)) == -1)
    {
        HandleError(errno,"Failed to bind to netlink socket: ");
        return;
    }

    //TODO: get initial information about interfaces using getifaddrs

    while(true)
    {
        //handle shutdown request
        if(shutdownRequested.load())
        {
            logger.Info()<<"Shuting down NetDevTracker worker thread"<<std::endl;
            break;
        }

        //wait for new data ready to be read from netlink
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sock, &set);
        auto t = timeout;
        auto rv = select(sock+1, &set, NULL, NULL, &t);
        if(rv==0)
            continue;
        if(rv<0)
        {
            auto error=errno;
            if(error==EINTR)//interrupted by signal
                break;
            HandleError(error,"Error awaiting message from netlink: ");
            return;
        }

        //from man netlink.7
        nlmsghdr buf[8192/sizeof(struct nlmsghdr)] = {};
        iovec iov = { buf, sizeof(buf) };
        sockaddr_nl sa = {};
        msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };

        //read message from netlink
        auto len = recvmsg(sock, &msg, 0);
        if(len<0)
        {
            auto error=errno;
            if(error==EINTR)//interrupted by signal
                break;
            HandleError(error,"Error reading message from netlink: ");
            return;
        }

        //process message
        for (auto *nh = (nlmsghdr*)buf; NLMSG_OK (nh, len) && nh->nlmsg_type != NLMSG_DONE; nh = NLMSG_NEXT (nh, len))
        {
            if (nh->nlmsg_type == NLMSG_ERROR)
            {
                HandleError(errno,"Error received from netlink: ");
                return;
            }
            else if (nh->nlmsg_type == RTM_NEWLINK || nh->nlmsg_type == RTM_DELLINK)
            {
                auto *ifl = (ifinfomsg*)NLMSG_DATA(nh);
                char msg_ifname[IFNAMSIZ];
                if_indextoname(ifl->ifi_index, msg_ifname);
                if(std::strncmp(ifname,msg_ifname,IFNAMSIZ)!=0)
                    continue; //interface name not matched
                if(!(ifl->ifi_flags&IFF_RUNNING))
                {
                    logger.Info()<<"Interface stopped"<<std::endl;
                    continue;
                }
                logger.Info()<<"Interface started"<<std::endl;
                if(ifl->ifi_flags&IFF_POINTOPOINT)
                {
                    logger.Info()<<"Interface point-to-point"<<std::endl;
                    continue;
                }
            }
            else if (nh->nlmsg_type == RTM_NEWADDR || nh->nlmsg_type == RTM_DELADDR)
            {
                auto *ifa = (ifaddrmsg*)NLMSG_DATA(nh);
                char msg_ifname[IFNAMSIZ];
                if_indextoname(ifa->ifa_index, msg_ifname);
                if(std::strncmp(ifname,msg_ifname,IFNAMSIZ)!=0)
                    continue; //interface name not matched

                auto rtl = IFA_PAYLOAD(nh);
                for (auto *rth = IFA_RTA(ifa); RTA_OK(rth, rtl); rth = RTA_NEXT(rth, rtl))
                {
                    if (rth->rta_type == IFA_LOCAL)
                    {
                        IPAddress ip(rth);
                        logger.Info()<<(nh->nlmsg_type==RTM_NEWADDR?"Added":"Removed")<<" local ip "<<ip<<std::endl;
                    }
                    if (rth->rta_type == IFA_ADDRESS)
                    {
                        IPAddress ip(rth);
                        logger.Info()<<(nh->nlmsg_type==RTM_NEWADDR?"Added":"Removed")<<" ip "<<ip<<std::endl;
                    }
                }
            }
            else logger.Warning()<<"Unknown message received, TODO: decode"<<std::endl;
        }
    }

    if(close(sock)!=0)
    {
        HandleError(errno,"Failed to close netlink socket: ");
        return;
    }
}
