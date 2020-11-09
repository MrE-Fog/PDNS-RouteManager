#ifndef PROTOBUFHELPER_H
#define PROTOBUFHELPER_H

#include "ILogger.h"

class ProtobufHelper
{
    private:
        ILogger &logger;
    public:
        ProtobufHelper(ILogger &logger);
        void Test(const char * const filename);
};

#endif // PROTOBUFHELPER_H
