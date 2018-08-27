
#include "common/Constants.h"
#include "libUtils/DetachedFunction.h"
#include <libUtils/SysCommand.h>
#include <string>

using namespace std;

unsigned int TxnSyncTimeout = 5;

void LaunchTxnSyncThread(const string& ipAddr)
{
    auto func = [](const string& ipAddr) {

        std::string rsyncTxnCommand
            = "rsync -az --no-whole-file --size-only -e \"ssh -o "
              "StrictHostKeyChecking=no\" ubuntu@"
            + ipAddr + ":" + REMOTE_TXN_DIR + "/ " + TXN_PATH;

        while (true)
        {
            LOG_GENERAL(INFO,
                        "[SyncTxn] "
                            << "Starting syncing");
            string out;
            if (!SysCommand::ExecuteCmdWithOutput(rsyncTxnCommand, out))
            {
                LOG_GENERAL(WARNING,
                            "Unable to launch command " << rsyncTxnCommand);
            }
            else
            {
                LOG_GENERAL(INFO, "Command Output " << out);
            }

            this_thread::sleep_for(chrono::seconds(TxnSyncTimeout));
        }
    };
    DetachedFunction(1, func, ipAddr);
}

int main() { LaunchTxnSyncThread(REMOTE_TXN_CREATOR_IP); }