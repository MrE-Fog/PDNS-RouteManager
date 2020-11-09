#include "StdioLogger.h"

#include "LogWriter.h"

StdioLogger::StdioLogger(const std::string &_name, const double &_initialTime, std::atomic<int> &_nameWD, std::mutex &_extLock):
    name(_name),
    initialTime(_initialTime),
    nameWD(_nameWD),
    extLock(_extLock)
{
}

static double GetTimeMark()
{
    timespec time={};
    clock_gettime(CLOCK_MONOTONIC,&time);
    return (double)time.tv_sec+(double)time.tv_nsec/(double)1000000000L;
}

LogWriter StdioLogger::Info()
{
    return LogWriter(std::cout,extLock,GetTimeMark()-initialTime,"INFO",4,name,nameWD.load());
}

LogWriter StdioLogger::Warning()
{
    return LogWriter(std::cout,extLock,GetTimeMark()-initialTime,"WARN",4,name,nameWD.load());
}

LogWriter StdioLogger::Error()
{
    return LogWriter(std::cerr,extLock,GetTimeMark()-initialTime,"ERR",4,name,nameWD.load());
}
