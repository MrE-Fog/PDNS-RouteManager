#ifndef INTERFACECONFIG_H
#define INTERFACECONFIG_H

#include "IPAddress.h"
#include <set>
#include <iostream>

class InterfaceConfig
{
    public:
        InterfaceConfig();
        InterfaceConfig(const InterfaceConfig &other);
        InterfaceConfig(const bool isUp, const bool isPtP, const std::set<IPAddress> &localIPs, const std::set<IPAddress> &remoteIPs);
        const bool isUp;
        const bool isPtP;
        const std::set<IPAddress> localIPs;
        const std::set<IPAddress> remoteIPs;

        InterfaceConfig AddLocalIP(const IPAddress &ip) const;
        InterfaceConfig DelLocalIP(const IPAddress &ip) const;
        InterfaceConfig AddRemoteIP(const IPAddress &ip) const;
        InterfaceConfig DelRemoteIP(const IPAddress &ip) const;
        InterfaceConfig SetState(const bool isUp) const;
        InterfaceConfig SetType(const bool isPtP) const;

        //TODO: add helper methods for checking current network-config sutable for adding non-blackhole routes:
        //for PtP interfaces - network is sutable for routes management if isUp==true in any case
        //for non PtP interfaces - network is sutable for routes management if we have non link-local IP addresses assigned to it

        //DO NOT ATTEMPT TO PERFORM ANY PENDING-ROUTES MANAGEMENT IF CURRENT NETWORK CONFIG IS NOT SUTABLE FOR ROUTES MANAGEMENT
        //blackhole-routes can be added in any state even if isUp==false

        friend std::ostream& operator<<(std::ostream& stream, const InterfaceConfig& target);
};

#endif // INTERFACECONFIG_H
