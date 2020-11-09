#ifndef STDIOLOGGER_H
#define STDIOLOGGER_H

#include "ILogger.h"

class StdioLogger final : public ILogger
{
    public:
        StdioLogger();
        std::ostream& Info() final;
        std::ostream& Warning() final;
        std::ostream& Error() final;
};

#endif // STDIOLOGGER_H
