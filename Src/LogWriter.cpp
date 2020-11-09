#include "LogWriter.h"
#include <memory>

LogWriter::LogWriter(std::ostream &_output, std::mutex &_extLock):
    output(_output),
    extLock(_extLock)
{
    extLock.lock();
    endl=false;
    //std::cout<<"!!!create!!!"<<std::endl;
}

LogWriter::~LogWriter()
{
    if(!endl)
        std::cout<<std::endl;
    else
        std::cout<<std::flush;
    //std::cout<<"!!!destruct!!!"<<std::endl;
    extLock.unlock();
}

LogWriter& LogWriter::operator<<(std::ostream& (*manip)(std::ostream&))
{
   if(manip==static_cast<std::ostream&(*)(std::ostream&)>(std::endl))
       endl=true;
    output<<manip;
    return *this;
}
