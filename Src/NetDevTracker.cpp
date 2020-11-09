#include "NetDevTracker.h"
#include <thread>
#include <chrono>

NetDevTracker::NetDevTracker(ILogger &_logger, IControl &control):
    WorkerBase(control),
    logger(_logger)
{

}

void NetDevTracker::RequestShutdown()
{

}

void NetDevTracker::Worker()
{
    std::this_thread::sleep_for(std::chrono::seconds(5));
    Control.Shutdown(0);
}

