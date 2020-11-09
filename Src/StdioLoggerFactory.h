#ifndef STDIOLOGGERFACTORY_H
#define STDIOLOGGERFACTORY_H

#include "ILogger.h"
#include <mutex>

class StdioLoggerFactory
{
    private:
        std::mutex stdioLock;
        timespec creationTime;
    public:
        StdioLoggerFactory();
        ILogger* CreateLogger(const char * const name);
        void DestroyLogger(ILogger* const target);
};

#endif // STDIOLOGGERFACTORY_H
