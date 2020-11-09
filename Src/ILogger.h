#ifndef ILOGGER_H
#define ILOGGER_H

#include <iostream>

class ILogger
{
    public:
        const std::ostream& Info;
        const std::ostream& Warning;
        const std::ostream& Error;
};

#endif // ILOGGER_H
