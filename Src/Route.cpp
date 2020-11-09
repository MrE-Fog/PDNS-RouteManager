#include "Route.h"
#include <functional>

Route::Route(const IPAddress &_dest, const uint64_t _expiration):
    dest(_dest),
    expiration(_expiration)
{
}

size_t Route::GetHashCode() const
{
    return (std::hash<uint64_t>{}(expiration))^dest.GetHashCode();
}

bool Route::Equals(const Route& other) const
{
    return dest.Equals(other.dest) && expiration==other.expiration;
}

bool Route::Less(const Route& other) const
{
    if(expiration<other.expiration)
        return true;
    if(expiration==other.expiration&&dest.Less(other.dest))
        return true;
    return false;
}

bool Route::Greater(const Route& other) const
{
    if(expiration>other.expiration)
        return true;
    if(expiration==other.expiration&&dest.Greater(other.dest))
        return true;
    return false;
}

bool Route::operator<(const Route& other) const
{
    return Less(other);
}

bool Route::operator==(const Route& other) const
{
    return Equals(other);
}

bool Route::operator>(const Route& other) const
{
    return Greater(other);
}

bool Route::operator>=(const Route& other) const
{
    return Greater(other)||Equals(other);
}

bool Route::operator<=(const Route& other) const
{
    return Less(other)||Equals(other);
}

std::ostream& operator<<(std::ostream& stream, const Route& target)
{
    stream<<"dest="<<target.dest<<",exp="<<target.expiration;
    return stream;
}


