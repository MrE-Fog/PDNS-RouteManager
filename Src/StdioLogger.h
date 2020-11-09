#ifndef STDIOLOGGER_H
#define STDIOLOGGER_H

#include "ILogger.h"
#include <mutex>

class StdioLogger : public ILogger
{
    private:
        const timespec &initialTime;
        const char * const loggerName;
        std::mutex &loggerLock;
    protected:
        StdioLogger(const timespec &initialTime, const char * const name, std::mutex &ioLock);
    public:
        std::ostream& Info() final;
        std::ostream& Warning() final;
        std::ostream& Error() final;
};

#endif // STDIOLOGGER_H
