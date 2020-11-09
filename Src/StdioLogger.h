#ifndef STDIOLOGGER_H
#define STDIOLOGGER_H

#include "ILogger.h"

class StdioLogger final : public ILogger
{
    public:
        StdioLogger();
};

#endif // STDIOLOGGER_H
