#include <iostream>
#include <signal.h>
#include "signals.h"

#include <unistd.h>

#include "Commands.h"

using namespace std;

void ctrlCHandler(int sig_num) {
    SmallShell& shell = SmallShell::getInstance();
    cout << "smash: got ctrl-C" << endl;
    const pid_t pid = shell.getLastFgCommandPid();
    if (kill(pid, SIGKILL) == -1) {
        return;
    } else {
        cout << "smash: process " << pid << " was killed" << endl;
    }
}
