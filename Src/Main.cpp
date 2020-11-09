#include "ILogger.h"
#include "IWorker.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"

#include <iostream>

using namespace std;

int usage(const char * const self)
{
    cerr << "Usage: " << self << " <listen ip-addr> <port> <target netdev>" << endl;
    return 1;
}

int main (int argc, char *argv[])
{
    if(argc!=3)
        return usage(argv[0]);
    try
    {
        //create main worker-instances
        NetDevTracker tracker;

        //init
        tracker.Startup();

        //TODO: wait for termination signal(s)

        //shutdown
        tracker.Shutdown();
    }
    catch (const exception& ex)
    {
        cerr << ex.what() << endl;
        return 1;
    }

    return  0;
}
