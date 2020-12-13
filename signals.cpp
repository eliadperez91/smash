#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"

using namespace std;

void ctrlZHandler(int sig_num) {
    std::cout << "smash: got ctrl-Z" << endl;
    SmallShell& smash = SmallShell::getInstance();
    JobsList* jobs = smash.getJobsListPtr();
    pid_t pid = jobs->getFgPid();
    if (pid == 0) { // no job in FG
        return;
    }
    if (kill((-1)*pid, SIGSTOP) == -1) { //signal to GROUP
        perror("smash error: kill failed");
        return;
    }
    JobsList::JobEntry* j = jobs->getJobByPid(pid);
    if (j == nullptr) {
        jobs->addJob(jobs->getFgCommandLine(), pid, true);
        j = jobs->getJobByPid(pid);
    } else { j->stop(); }
    j->resetSecondsElapsed();
    jobs->clearFgCommand();
    std::cout << "smash: process " << pid << " was stopped" << endl;
}

void ctrlCHandler(int sig_num) {
    std::cout << "smash: got ctrl-C" << endl;
    SmallShell& smash = SmallShell::getInstance();
    JobsList* jobs = smash.getJobsListPtr();
    pid_t pid = jobs->getFgPid();
    if (pid == 0) { // no job in FG
        return;
    }
    if (kill((-1)*pid, SIGKILL) == -1) { //signal to GROUP
        perror("smash error: kill failed");
        return;
    }
    JobsList::JobEntry* j = jobs->getJobByPid(pid);
    if (j != nullptr) {
        jobs->removeJob(pid);
    }
    jobs->clearFgCommand();
    std::cout << "smash: process " << pid << " was killed" << endl;
}

void alarmHandler(int sig_num) {
    std::cout << "smash: got an alarm" << endl;
    SmallShell &smash = SmallShell::getInstance();
    smash.getJobsListPtr()->removeFinishedJobs();
    SmallShell::TimeoutEntry t = smash.popTimedoutEntry();
    pid_t pid = t.getPid();
    if (pid != 0) {
        if (kill((-1) * pid, SIGKILL) == -1) { //signal to GROUP
            if (errno != ESRCH) { //unexpected error
                perror("smash error: kill failed");
                return;
            }
        } else { //signal sent successfully
            std::cout << "smash: " << t.getCommandLine() << " timed out!" << endl;
        }
    }
    smash.setNewAlarm();
}
