#include "WorkerBase.h"

bool WorkerBase::Startup()
{
    const std::lock_guard<std::mutex> lock(workerLock);
    if(worker!=nullptr)
        return false;
    worker=new std::thread(&WorkerBase::Worker,this);
    return true;
}

bool WorkerBase::Shutdown()
{
    const std::lock_guard<std::mutex> lock(workerLock);
    if(worker==nullptr)
        return false;
    OnShutdown();
    worker->join();
    delete worker;
    worker=nullptr;
    return true;
}

bool WorkerBase::RequestShutdown()
{
    const std::lock_guard<std::mutex> lock(workerLock);
    if(worker==nullptr)
        return false;
    OnShutdown();
    return true;
}
