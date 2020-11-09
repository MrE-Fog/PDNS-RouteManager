#ifndef IPADDRESS_H
#define IPADDRESS_H

#include <iostream>

#include <linux/rtnetlink.h>

#define IPV4_ADDR_LEN 4
#define IPV6_ADDR_LEN 16
#define IP_ADDR_LEN IPV6_ADDR_LEN

class IPAddress
{
    public:
        IPAddress();
        IPAddress(const sockaddr * const sa);
        IPAddress(const IPAddress &other);
        IPAddress(const rtattr * const rta);
        bool Equals(const IPAddress &other) const;
        bool Less(const IPAddress &other) const;
        bool Greater(const IPAddress &other) const;
        bool operator<(const IPAddress &other) const;
        bool operator==(const IPAddress &other) const;
        bool operator>(const IPAddress &other) const;
        bool operator>=(const IPAddress &other) const;
        bool operator<=(const IPAddress &other) const;

        friend std::ostream& operator<<(std::ostream& stream, const IPAddress& target);

        const bool isValid;
        const bool isV6;
    private:
        struct RawIP
        {
            RawIP();
            RawIP(const void * const source, const size_t len);
            unsigned char data[IP_ADDR_LEN];
        };
        const RawIP ip;
};

#endif // IPADDRESS_H
