#include "SignalHandler.h"

#include <csignal>
#include <atomic>

static std::atomic<bool> signalReceived;

void SignalHandler::Setup()
{
    std::signal(SIGINT, OnSignal);
    std::signal(SIGHUP, OnSignal);
    std::signal(SIGTERM, OnSignal);
}

bool SignalHandler::IsSignalReceived()
{
    return signalReceived.load();
}

void SignalHandler::OnSignal(int)
{
    signalReceived.store(true);
}
