#include "ILogger.h"
#include "IWorker.h"
#include "StdioLogger.h"
#include "NetDevTracker.h"

#include <iostream>

using namespace std;

int usage(ILogger &logger, const char * const self)
{
    logger.Error() << "Usage: " << self << " <listen ip-addr> <port> <target netdev>" << endl;
    return 1;
}

int main (int argc, char *argv[])
{
    StdioLogger logger;

    if(argc!=3)
        return usage(logger,argv[0]);
    try
    {
        //create main worker-instances
        NetDevTracker tracker(logger);

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
