#include "StdioLoggerFactory.h"

#include "StdioLogger.h"

class FinalStdioLogger final : public StdioLogger
{
    public:
        FinalStdioLogger(const std::string &_name, const double &_time, std::atomic<unsigned int> &_nameWD, std::mutex &_extLock): StdioLogger(_name, _time, _nameWD, _extLock) {};
};

StdioLoggerFactory::StdioLoggerFactory()
{
    maxNameWD.store(1);
    timespec time={};
    clock_gettime(CLOCK_MONOTONIC,&time);
    creationTime=static_cast<double>(time.tv_sec)+static_cast<double>(time.tv_nsec)/1000000000.;
}

ILogger * StdioLoggerFactory::CreateLogger(const std::string &name)
{
    if(name.length()>maxNameWD.load())
        maxNameWD.store(static_cast<unsigned int>(name.length()));
    return new FinalStdioLogger(name,creationTime,maxNameWD,stdioLock);
}

void StdioLoggerFactory::DestroyLogger(ILogger * const target)
{
    if(target==nullptr||dynamic_cast<FinalStdioLogger*>(target)==nullptr)
        return;
    delete dynamic_cast<FinalStdioLogger*>(target);
}
