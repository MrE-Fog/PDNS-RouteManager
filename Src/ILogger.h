#ifndef ILOGGER_H
#define ILOGGER_H

#include <iostream>

class ILogger
{
    public:
        virtual std::ostream& Info()=0;
        virtual std::ostream& Warning()=0;
        virtual std::ostream& Error()=0;
};

#endif // ILOGGER_H
