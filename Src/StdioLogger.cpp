#include "StdioLogger.h"

//TODO: add ability to set max verbosity level
//TODO: customize ostreams somehow, to include extra header on every new line [TIME: MESSAGE TYPE]
StdioLogger::StdioLogger() { }

std::ostream& StdioLogger::Info()
{
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
