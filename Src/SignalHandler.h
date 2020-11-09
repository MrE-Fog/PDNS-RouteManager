#ifndef SIGNALHANDLER_H
#define SIGNALHANDLER_H

class SignalHandler
{
    private:
        SignalHandler() {}; //we have a "static" class here
        static void OnSignal(int);
    public:
        static void Setup();
        static bool IsSignalReceived();
};

#endif // SIGNALHANDLER_H
