#include "StdioLoggerFactory.h"

#include "StdioLogger.h"

class FinalStdioLogger final : public StdioLogger
{
    public:
        FinalStdioLogger(const std::string &_name, const double &_time, std::atomic<int> &_nameWD, std::mutex &_extLock): StdioLogger(_name, _time, _nameWD, _extLock) {};
};

StdioLoggerFactory::StdioLoggerFactory()
{
    maxNameWD.store(1);
    timespec time={};
    clock_gettime(CLOCK_MONOTONIC,&time);
    creationTime=(double)time.tv_sec+(double)time.tv_nsec/(double)1000000000L;
}

ILogger * StdioLoggerFactory::CreateLogger(const std::string &name)
{
    if(name.length()>(unsigned)maxNameWD.load())
        maxNameWD.store((signed)name.length());
    return new FinalStdioLogger(name,creationTime,maxNameWD,stdioLock);
}

void StdioLoggerFactory::DestroyLogger(ILogger * const target)
{
    if(target==nullptr||dynamic_cast<FinalStdioLogger*>(target)==nullptr)
        return;
    delete dynamic_cast<FinalStdioLogger*>(target);
}
