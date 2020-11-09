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
        LogWriter Info() final;
        LogWriter Warning() final;
        LogWriter Error() final;
};

#endif // STDIOLOGGER_H
