#ifndef IWORKER_H
#define IWORKER_H

#include "thread"
#include "mutex"

class WorkerBase
{
    private:
        std::mutex workerLock;
        std::thread* worker = nullptr;
    protected:
        virtual void Worker() = 0; // main worker's logic will run in a separate thread started by Startup method
        virtual void OnShutdown() = 0; // must be threadsafe and notify Worker to stop, will be called from Shutdown that itself may be called from any thread
    public:
        bool Startup(); //start separate thread from Worker method. Startup may be called from any thread
        bool Shutdown(); //stop previously started thread, by invoking OnShutdown and awaiting Worker thread to complete
        bool RequestShutdown(); //same as Shutdown, but only invoke OnShutdown and do not wait. Shutdown should be called next to collect thread
};

#endif // IWORKER_H
