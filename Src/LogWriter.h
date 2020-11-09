#ifndef LOGWRITER_H
#define LOGWRITER_H

#include <iostream>
#include <mutex>

class LogWriter
{
    private:
        std::ostream &output;
        std::mutex &extLock;
        bool endl;
    public:
        LogWriter(std::ostream &output, std::mutex &extLock);
        LogWriter (LogWriter&) = delete;
        LogWriter (LogWriter&&) = default;
        ~LogWriter();
        template <class T> LogWriter& operator<<(T&& x) { endl=false; output<<std::forward<T>(x); return *this; }
        LogWriter& operator<<(std::ostream& (*manip)(std::ostream&)); //override for manipulators, detects use of endl at the end
};

#endif // LOGWRITER_H
