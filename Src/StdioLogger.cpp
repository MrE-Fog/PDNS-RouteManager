#include "StdioLogger.h"

/*class LogStream: public std::streambuf, public std::ostream
{
    private:
        std::ostream &output;
    public:
        LogStream(std::ostream &_output, int x): output(_output)
        {
            //TODO: write header
        }

        template <class T>
        LogStream& operator<<(T&& x)
        {
            output<<std::forward<T>(x);
            return *this;
        }

        LogStream& operator<<(std::ostream& (*manip)(std::ostream&))
        {
            output<<manip;
            return *this;
        }
};*/

StdioLogger::StdioLogger(const timespec &_initialTime, const char * const name, std::mutex& ioLock):
    initialTime(_initialTime),
    loggerName(name),
    loggerLock(ioLock)
{

}

std::ostream& StdioLogger::Info()
{
/*    LogStream x(std::cout, 1);
    return std::move(static_cast<std::ostream>(x));*/
    return std::cout;
}

std::ostream& StdioLogger::Warning()
{
    return std::cout;
}

std::ostream& StdioLogger::Error()
{
    return std::cerr;
}
