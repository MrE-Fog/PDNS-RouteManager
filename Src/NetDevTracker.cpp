#include "NetDevTracker.h"
#include "IPAddress.h"
#include "InterfaceConfig.h"
#include "ImmutableStorage.h"

#include <thread>
#include <chrono>
#include <cstring>
#include <cerrno>

#include <unistd.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

class ShutdownMessage: public IShutdownMessage { public: ShutdownMessage(int _ec):IShutdownMessage(_ec){} };
class NetDevUpdateMessage: public INetDevUpdateMessage { public: NetDevUpdateMessage(InterfaceConfig _config):INetDevUpdateMessage(_config){} };
class RouteAddedMessage: public IRouteAddedMessage { public: RouteAddedMessage(const IPAddress &_ip):IRouteAddedMessage(_ip){} };
class RouteRemovedMessage: public IRouteRemovedMessage { public: RouteRemovedMessage(const IPAddress &_ip):IRouteRemovedMessage(_ip){} };

NetDevTracker::NetDevTracker(ILogger &_logger, IMessageSender &_sender, const std::string &_ifname, const timeval _timeout, const int _metric):
    ifname(_ifname),
    timeout(_timeout),
    metric(_metric),
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

#define ISPTP(ifa) ((ifa->ifa_flags&IFF_POINTOPOINT)!=0)
#define ISUP(ifa) ((ifa->ifa_flags&(IFF_UP|IFF_RUNNING))!=0)
#define ISBRC(ifa) ((ifa->ifa_flags&IFF_BROADCAST)!=0)

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
    nlAddr.nl_groups=RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_ROUTE;

    if (bind(sock, (sockaddr *)&nlAddr, sizeof(nlAddr)) == -1)
    {
        HandleError(errno,"Failed to bind to netlink socket: ");
        return;
    }

    auto cfgStorage=ImmutableStorage<InterfaceConfig>(InterfaceConfig());

    ifaddrs *ifaddr=nullptr;
    if(getifaddrs(&ifaddr)!=0)
    {
        HandleError(errno,"Failed while executing getifaddrs: ");
        return;
    }

    bool ifFound=false;
    bool isUP=true;
    bool isPtP=false;

    for(auto ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if(ifa->ifa_addr==NULL)
            continue; //no address
        if(ifa->ifa_addr->sa_family!=AF_INET && ifa->ifa_addr->sa_family!=AF_INET6)
            continue; //unsupported interface family
        if(std::strncmp(ifname.c_str(),ifa->ifa_name,IFNAMSIZ)!=0)
            continue; //interface name not matched

        ifFound=true;
        //add local ip for interface
        IPAddress localIP(ifa->ifa_addr);
        cfgStorage.Set(cfgStorage.Get().AddLocalIP(localIP));

        //set remote address - either broadcast or ptp
        if(ISPTP(ifa))
            cfgStorage.Set(cfgStorage.Get().AddRemoteIP(IPAddress(ifa->ifa_dstaddr)));
        else if(ISBRC(ifa)&&!localIP.isV6)
            cfgStorage.Set(cfgStorage.Get().AddRemoteIP(IPAddress(ifa->ifa_broadaddr)));

        //update interface flags
        isPtP|=ISPTP(ifa);
        isUP&=ISUP(ifa);
    }
    isUP&=ifFound;

    freeifaddrs(ifaddr);
    cfgStorage.Set(cfgStorage.Get().SetType(isPtP).SetState(isUP));

    logger.Info()<<"Initial interface state: "<<cfgStorage.Get()<<std::endl;
    sender.SendMessage(this,NetDevUpdateMessage(cfgStorage.Get()));

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

        cfgStorage.isUpdated=false;

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
                char msg_ifname[IFNAMSIZ]={};
                if_indextoname(ifl->ifi_index, msg_ifname);
                if(std::strncmp(ifname.c_str(),msg_ifname,IFNAMSIZ)!=0)
                    continue; //interface name not matched
                if(nh->nlmsg_type == RTM_DELLINK) //link disappeared, set state to false
                    cfgStorage.Set(cfgStorage.Get().SetState(false)); //NOTE: TODO: maybe we also need to update interface type with SetType
                else //network device was created or updated
                    cfgStorage.Set(cfgStorage.Get().SetState((ifl->ifi_flags&(IFF_UP|IFF_RUNNING))!=0).SetType((ifl->ifi_flags&IFF_POINTOPOINT)!=0));
            }
            else if (nh->nlmsg_type == RTM_NEWADDR || nh->nlmsg_type == RTM_DELADDR)
            {
                auto *ifa = (ifaddrmsg*)NLMSG_DATA(nh);
                char msg_ifname[IFNAMSIZ]={};
                if_indextoname(ifa->ifa_index, msg_ifname);
                if(std::strncmp(ifname.c_str(),msg_ifname,IFNAMSIZ)!=0)
                    continue; //interface name not matched
                //mark config as updated for now, even if no changes in addresses was performed
                cfgStorage.isUpdated=true;
                auto rtl = IFA_PAYLOAD(nh);
                for (auto *rth = IFA_RTA(ifa); RTA_OK(rth, rtl); rth = RTA_NEXT(rth, rtl))
                {
                    auto config=cfgStorage.Get();
                    if(rth->rta_type == IFA_LOCAL)
                        cfgStorage.Set(nh->nlmsg_type==RTM_NEWADDR?config.AddLocalIP(IPAddress(rth)):config.DelLocalIP(IPAddress(rth)));
                    else if(rth->rta_type == IFA_BROADCAST)
                        cfgStorage.Set(nh->nlmsg_type==RTM_NEWADDR?config.AddRemoteIP(IPAddress(rth)):config.DelRemoteIP(IPAddress(rth)));
                    else if(rth->rta_type == IFA_ADDRESS && !config.isPtP)
                        cfgStorage.Set(nh->nlmsg_type==RTM_NEWADDR?config.AddLocalIP(IPAddress(rth)):config.DelLocalIP(IPAddress(rth)));
                    else if(rth->rta_type == IFA_ADDRESS && config.isPtP)
                        cfgStorage.Set(nh->nlmsg_type==RTM_NEWADDR?config.AddRemoteIP(IPAddress(rth)):config.DelRemoteIP(IPAddress(rth)));
                }
            }
            else if (nh->nlmsg_type == RTM_NEWROUTE || nh->nlmsg_type == RTM_DELROUTE)
            {
                auto *rtm = (rtmsg*)NLMSG_DATA(nh);
                //we only need to track routes with these properties
                if((rtm->rtm_family!=AF_INET&&rtm->rtm_family!=AF_INET6)||rtm->rtm_table!=RT_TABLE_MAIN||rtm->rtm_scope!=RT_SCOPE_UNIVERSE||rtm->rtm_type!=RTN_UNICAST||rtm->rtm_protocol!=RTPROT_STATIC)
                    continue;
                //to identify route installed/removed by this program - we need to get following attributes
                auto dest=ImmutableStorage<IPAddress>(IPAddress()); //destination ip address - valid ipv4 or ipv6 address
                char rt_ifname[IFNAMSIZ]={}; //interface (must match)
                int rt_metric=-1; //metric/priority (must match)
                //process rtattr attributes
                auto rtl = RTM_PAYLOAD(nh);
                for (auto *rth = RTM_RTA(rtm); RTA_OK(rth, rtl); rth = RTA_NEXT(rth, rtl))
                {
                    if(rth->rta_type==RTA_DST)
                        dest.Set(IPAddress(rth));
                    else if(rth->rta_type==RTA_OIF)
                    {
                        uint ifIdx=0;
                        memcpy((void*)&ifIdx,RTA_DATA(rth),sizeof(uint));
                        if(if_indextoname(ifIdx,rt_ifname)==nullptr)
                            logger.Warning()<<"Failed to decode interface name while parsing route "<<(nh->nlmsg_type==RTM_NEWROUTE?"added":"removed")<<" notification:"<<strerror(errno)<<std::endl;
                    }
                    else if(rth->rta_type==RTA_PRIORITY)
                        memcpy((void*)&rt_metric,RTA_DATA(rth),sizeof(int));
                }
                if(metric!=rt_metric||std::strncmp(ifname.c_str(),rt_ifname,IFNAMSIZ)!=0)
                    continue; //metric/priority or interface name not matched
                if(!dest.Get().isValid)
                {
                    logger.Warning()<<"No valid destination address received for route "<<(nh->nlmsg_type==RTM_NEWROUTE?"added":"removed")<<" notification"<<std::endl;
                    continue;
                }
                //logger.Info()<<"Route "<<(nh->nlmsg_type==RTM_NEWROUTE?"added":"removed")<<"; ip="<<dest.Get()<<std::endl;
                if(nh->nlmsg_type==RTM_NEWROUTE)
                    sender.SendMessage(this,RouteAddedMessage(dest.Get()));
                else
                    sender.SendMessage(this,RouteRemovedMessage(dest.Get()));
            }
            else logger.Warning()<<"Unknown message received: "<<nh->nlmsg_type<<std::endl; //TODO: decode other messages
        }

        if(cfgStorage.isUpdated)
        {
            auto config=cfgStorage.Get();
            logger.Info()<<"Interface state updated: "<<config<<std::endl;
            sender.SendMessage(this,NetDevUpdateMessage(config));
        }
    }

    if(close(sock)!=0)
    {
        HandleError(errno,"Failed to close netlink socket: ");
        return;
    }
}
