#include "StdioLogger.h"

using namespace std;

//TODO: add ability to set max verbosity level
//TODO: customize ostreams somehow, to include extra header on every new line [TIME: MESSAGE TYPE]
StdioLogger::StdioLogger() { }

ostream& StdioLogger::Info()
{
    return cout;
}

ostream& StdioLogger::Warning()
{
    return cout;
}

ostream& StdioLogger::Error()
{
    return cerr;
}
