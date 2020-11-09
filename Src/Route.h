#ifndef ROUTE_H
#define ROUTE_H

#include "IPAddress.h"

class Route
{
    public:
        Route(const IPAddress &dest, const uint64_t expiration);

        const IPAddress dest;
        const uint64_t expiration;

        size_t GetHashCode() const;
        bool Equals(const Route &other) const;
        bool Less(const Route &other) const;
        bool Greater(const Route &other) const;
        bool operator<(const Route &other) const;
        bool operator==(const Route &other) const;
        bool operator>(const Route &other) const;
        bool operator>=(const Route &other) const;
        bool operator<=(const Route &other) const;

        friend std::ostream& operator<<(std::ostream &stream, const Route &target);
};

namespace std { template<> struct hash<Route>{ size_t operator()(const Route &target) const {return target.GetHashCode();}}; }

#endif // ROUTE_H
