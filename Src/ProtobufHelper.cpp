#include "ProtobufHelper.h"
#include "dnsmessage.pb.h"

#include <fstream>
#include <iostream>
#include <exception>

uint16_t ProtobufHelper::SwapU16(uint16_t& src) const
{
    return useByteSwap?(uint16_t)((src&0x00FF)<<8|(src&0xFF00)>>8):src;
}

ProtobufHelper::ProtobufHelper(ILogger& _logger, bool _useByteSwap):
    logger(_logger),
    useByteSwap(_useByteSwap)
{
}

void ProtobufHelper::Test(const char* const filename)
{
    logger.Info()<<"reading from "<<filename<<std::endl;
    std::fstream file(filename, std::ios::in | std::ios::binary);
    if(!file.is_open())
         logger.Error()<<"file not opened!"<<std::endl;
    uint16_t len;
    file.read((char*)(&len),2);
    len=SwapU16(len);
    char msgBuff[8192];
    file.read(msgBuff,len);
    PBDNSMessage message;
    auto result=message.ParseFromArray(msgBuff,len);
    logger.Info()<<"result "<<result<<std::endl;
}
