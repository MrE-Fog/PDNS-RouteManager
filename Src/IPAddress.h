#ifndef IPADDRESS_H
#define IPADDRESS_H

#include <iostream>
#include <linux/rtnetlink.h>

#define IPV4_ADDR_LEN 4
#define IPV6_ADDR_LEN 16
#define IP_ADDR_LEN IPV6_ADDR_LEN

class IPAddress
{
    private:
        const bool isValid;
        const bool isV6;
        unsigned char ip[IP_ADDR_LEN];
    public:
        IPAddress();
        IPAddress(const IPAddress &other);
        IPAddress(const rtattr * const rta);
        bool Equals(const IPAddress &other);
        friend std::ostream& operator<<(std::ostream& stream, const IPAddress& target);
};

#endif // IPADDRESS_H
