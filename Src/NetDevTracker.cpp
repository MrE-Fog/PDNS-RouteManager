#include "NetDevTracker.h"
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

NetDevTracker::NetDevTracker(ILogger &_logger, IControl &_control, const timeval &_timeout):
    WorkerBase(_control),
    timeout(_timeout),
    logger(_logger)
{
    shutdownRequested.store(false);
}

void NetDevTracker::RequestShutdown()
{
    logger.Info()<<"Requesting shutdown for NetDevTracker worker thread"<<std::endl;
    shutdownRequested.store(true);
}

void NetDevTracker::Worker()
{
    logger.Info()<<"NetDevTracker worker thread started"<<std::endl;

    auto sock=socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if(sock==-1)
    {
        auto error=errno;
        logger.Error() << "Failed to open netlink socket: " << strerror(error) << std::endl;
        control.Shutdown(error);
    }

    sockaddr_nl addr = {};
    addr.nl_family=AF_NETLINK;
    addr.nl_groups=RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        auto error=errno;
        logger.Error() << "Failed to bind to netlink socket: " << strerror(error) << std::endl;
        control.Shutdown(error);
    }

    //from man netlink.7
    nlmsghdr buf[8192/sizeof(struct nlmsghdr)];
    iovec iov = { buf, sizeof(buf) };
    sockaddr_nl sa;
    msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };

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
            logger.Error() << "Error awaiting message from netlink : " << strerror(error) << std::endl;
            control.Shutdown(error);
        }

        auto len = recvmsg(sock, &msg, 0);
        for (nlmsghdr *nh = (struct nlmsghdr *) buf; NLMSG_OK (nh, len); nh = NLMSG_NEXT (nh, len))
        {
            if (nh->nlmsg_type == NLMSG_DONE) // The end of multipart message
                break;
            if (nh->nlmsg_type == NLMSG_ERROR)
            {
                auto error=errno;
                logger.Error() << "Error received from netlink : " << strerror(error) << std::endl;
                control.Shutdown(error);
            }
            logger.Warning() << "Message received, TODO: decode" << std::endl;
        }
    }

    if(close(sock)!=0)
    {
        auto error=errno;
        logger.Error() << "Failed to close netlink socket: " << strerror(error) << std::endl;
        control.Shutdown(error);
    }
}
