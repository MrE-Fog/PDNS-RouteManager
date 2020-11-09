#ifndef IWORKER_H
#define IWORKER_H

class IWorker
{
    public:
        virtual void Startup() = 0;
        virtual void Shutdown() = 0;
};

#endif // IWORKER_H
