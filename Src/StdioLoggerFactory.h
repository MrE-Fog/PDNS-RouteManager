#ifndef STDIOLOGGERFACTORY_H
#define STDIOLOGGERFACTORY_H

#include "ILogger.h"
#include <mutex>
#include <atomic>

class StdioLoggerFactory
{
    private:
        std::atomic<unsigned int> maxNameWD;
        std::mutex stdioLock;
        double creationTime;
    public:
        StdioLoggerFactory();
        ILogger* CreateLogger(const std::string &name);
        void DestroyLogger(ILogger* const target);
};

#endif // STDIOLOGGERFACTORY_H
