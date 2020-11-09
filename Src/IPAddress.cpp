#include "IPAddress.h"

#include <cstring>
#include <arpa/inet.h>

IPAddress::IPAddress():
    isValid(false),
    isV6(false)
{
}

IPAddress::IPAddress(const IPAddress& other):
    isValid(other.isValid),
    isV6(other.isV6)
{
    std::memcpy((void*)ip,(const void*)other.ip,IP_ADDR_LEN);
}

IPAddress::IPAddress(const rtattr* const rta):
    isValid(RTA_PAYLOAD(rta)==IPV6_ADDR_LEN || RTA_PAYLOAD(rta)==IPV4_ADDR_LEN),
    isV6(RTA_PAYLOAD(rta)==IPV6_ADDR_LEN)
{
    if(!isValid)
        return;
    std::memcpy((void*)ip,RTA_DATA(rta),RTA_PAYLOAD(rta));
}

bool IPAddress::Equals(const IPAddress& other)
{
    if(!other.isValid || isV6!=other.isV6)
        return false;
    return (std::memcmp(ip,other.ip,isV6?IPV6_ADDR_LEN:IPV4_ADDR_LEN)==0);
}

std::ostream& operator<<(std::ostream& stream, const IPAddress& target)
{
    auto resultLen=target.isV6?INET6_ADDRSTRLEN:INET_ADDRSTRLEN;
    char result[INET6_ADDRSTRLEN>INET_ADDRSTRLEN?INET6_ADDRSTRLEN:INET_ADDRSTRLEN];
    inet_ntop(target.isV6?AF_INET6:AF_INET, target.ip, result, resultLen);
    stream << result;
    return stream;
}
