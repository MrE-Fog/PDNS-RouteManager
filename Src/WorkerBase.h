#ifndef IWORKER_H
#define IWORKER_H

#include "IControl.h"
#include "thread"
#include "mutex"

class WorkerBase
{
    private:
        std::mutex workerLock;
        std::thread* worker = nullptr;
    protected:
        IControl& control;
        WorkerBase(IControl &control);
        virtual void Worker() = 0; // main worker's logic will run in a separate thread started by Startup method
        virtual void RequestShutdown() = 0; // must be threadsafe and notify Worker to stop, will be called inside Shutdown that itself may be called from any thread
    public:
        bool Startup(); //start separate thread from Worker method. Startup may be called from any thread
        bool Shutdown(); //stop previously started thread, by invoking RequestShutdown and awaiting Worker thread to complete
};

#endif // IWORKER_H
