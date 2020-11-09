#ifndef INTERFACECONFIG_H
#define INTERFACECONFIG_H

#include "IPAddress.h"
#include <set>

class InterfaceConfig
{
    public:
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
};

#endif // INTERFACECONFIG_H
