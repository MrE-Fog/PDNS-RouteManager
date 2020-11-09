#ifndef PROTOBUFHELPER_H
#define PROTOBUFHELPER_H

#include "ILogger.h"

class ProtobufHelper
{
    private:
        ILogger &logger;
        const bool useByteSwap;
        uint16_t SwapU16(uint16_t &src) const;
    public:
        ProtobufHelper(ILogger &logger, bool useByteSwap);
        void Test(const char * const filename);
};

#endif // PROTOBUFHELPER_H
