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
        unsigned char ip[IP_ADDR_LEN];
    public:
        const bool isValid;
        const bool isV6;

        IPAddress();
        IPAddress(const sockaddr * const sa);
        IPAddress(const IPAddress &other);
        IPAddress(const rtattr * const rta);
        bool Equals(const IPAddress &other);

        friend std::ostream& operator<<(std::ostream& stream, const IPAddress& target);
};

#endif // IPADDRESS_H
