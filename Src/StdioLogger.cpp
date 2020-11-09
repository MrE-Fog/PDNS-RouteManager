#include "StdioLogger.h"

#include "LogWriter.h"

StdioLogger::StdioLogger(const timespec &_initialTime, const char * const name, std::mutex& ioLock):
    initialTime(_initialTime),
    loggerName(name),
    loggerLock(ioLock)
{

}

LogWriter StdioLogger::Info()
{
    return LogWriter(std::cout,loggerLock);
}

LogWriter StdioLogger::Warning()
{
    return LogWriter(std::cout,loggerLock);
}

LogWriter StdioLogger::Error()
{
    return LogWriter(std::cerr,loggerLock);
}
