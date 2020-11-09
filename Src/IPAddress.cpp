#include "IPAddress.h"

#include <cstring>
#include <arpa/inet.h>

IPAddress::RawIP::RawIP()
{
    std::memset((void*)data,0,IP_ADDR_LEN);
}

IPAddress::RawIP::RawIP(const void* const source, const size_t len) : RawIP()
{
    std::memcpy((void*)data,source,len);
}

IPAddress::IPAddress():
    isValid(false),
    isV6(false),
    ip(RawIP())
{
}

IPAddress::IPAddress(const sockaddr* const sa):
    isValid(sa->sa_family==AF_INET||sa->sa_family==AF_INET6),
    isV6(sa->sa_family==AF_INET6),
    ip(!isValid?RawIP():RawIP(isV6?(const void*)&(((const sockaddr_in6*)sa)->sin6_addr):(const void*)&(((const sockaddr_in*)sa)->sin_addr),isV6?IPV6_ADDR_LEN:IPV4_ADDR_LEN))
{
}

IPAddress::IPAddress(const IPAddress& other):
    isValid(other.isValid),
    isV6(other.isV6),
    ip(other.ip)
{
}

IPAddress::IPAddress(const rtattr* const rta):
    isValid(RTA_PAYLOAD(rta)==IPV6_ADDR_LEN || RTA_PAYLOAD(rta)==IPV4_ADDR_LEN),
    isV6(RTA_PAYLOAD(rta)==IPV6_ADDR_LEN),
    ip(!isValid?RawIP():RawIP(RTA_DATA(rta),RTA_PAYLOAD(rta)))
{
}

IPAddress::IPAddress(const std::string& rdata):
    isValid(rdata.length()==IPV6_ADDR_LEN || rdata.length()==IPV4_ADDR_LEN),
    isV6(rdata.length()==IPV6_ADDR_LEN),
    ip(!isValid?RawIP():RawIP(rdata.data(),rdata.length()))
{
}

bool IPAddress::Equals(const IPAddress& other) const
{
    return isValid==other.isValid && isV6==other.isV6 && std::memcmp(ip.data,other.ip.data,IP_ADDR_LEN)==0;
}

bool IPAddress::Less(const IPAddress& other) const
{
    //first, compare the flags
    if((10*isValid+1*isV6) < (10*other.isValid+1*other.isV6))
        return true;
    if(std::memcmp(ip.data,other.ip.data,IP_ADDR_LEN)<0)
        return true;
    return false;
}

bool IPAddress::Greater(const IPAddress& other) const
{
    if((10*isValid+1*isV6) > (10*other.isValid+1*other.isV6))
        return true;
    if(std::memcmp(ip.data,other.ip.data,IP_ADDR_LEN)>0)
        return true;
    return false;
}

bool IPAddress::operator<(const IPAddress &other) const
{
    return Less(other);
}

bool IPAddress::operator==(const IPAddress& other) const
{
    return Equals(other);
}

bool IPAddress::operator>(const IPAddress& other) const
{
    return Greater(other);
}

bool IPAddress::operator>=(const IPAddress& other) const
{
    return Greater(other)||Equals(other);
}

bool IPAddress::operator<=(const IPAddress& other) const
{
    return Less(other)||Equals(other);
}

std::ostream& operator<<(std::ostream& stream, const IPAddress& target)
{
    auto resultLen=target.isV6?INET6_ADDRSTRLEN:INET_ADDRSTRLEN;
    char result[INET6_ADDRSTRLEN>INET_ADDRSTRLEN?INET6_ADDRSTRLEN:INET_ADDRSTRLEN];
    inet_ntop(target.isV6?AF_INET6:AF_INET, target.ip.data, result, resultLen);
    stream << result;
    return stream;
}

