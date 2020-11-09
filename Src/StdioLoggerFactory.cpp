#include "StdioLoggerFactory.h"

#include "StdioLogger.h"

class FinalStdioLogger final : public StdioLogger
{
    public:
        FinalStdioLogger(const timespec &time, const char * const name, std::mutex &extLock): StdioLogger(time, name, extLock) {};
};

StdioLoggerFactory::StdioLoggerFactory()
{
    clock_gettime(CLOCK_MONOTONIC,&creationTime);
}

ILogger * StdioLoggerFactory::CreateLogger(const char* const name)
{
    return new FinalStdioLogger(creationTime,name,stdioLock);
}

void StdioLoggerFactory::DestroyLogger(ILogger * const target)
{
    if(target==nullptr||dynamic_cast<FinalStdioLogger*>(target)==nullptr)
        return;
    delete dynamic_cast<FinalStdioLogger*>(target);
}
