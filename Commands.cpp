#include <unistd.h>
#include <cstring>
#include <iostream>
#include <algorithm>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));


string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}


bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  size_t idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)));
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = nullptr;
  }
  return i;

  FUNC_EXIT()
}

// TODO: Add your implementation for classes in Commands.h 

Command::Command(const char* cmd_line) : cmd_line(string(cmd_line)), 
				argc(0) {
		char cmd_line_copy[strlen(cmd_line)+1];
		strcpy(cmd_line_copy, cmd_line);
		_removeBackgroundSign(cmd_line_copy);
		argc = _parseCommandLine(cmd_line_copy, argv);
	}

Command::~Command() {
	for (int i = 0; argv[i] != nullptr; ++i) {
		free(argv[i]);
	}
}

BuiltInCommand::BuiltInCommand(const char* cmd_line) : 
				Command(cmd_line) {}
				
ChangePromptCommand::ChangePromptCommand(const char* cmd_line, char** pPrompt) :
				BuiltInCommand(cmd_line), pPrompt(pPrompt) {}
				
void ChangePromptCommand::setPrompt(const char* prompt) { // free and realloc prompt (private data member of SmallShell class)
	free(*pPrompt);
	if (prompt == nullptr) {
		*pPrompt = nullptr;
		return; 
	}
	*pPrompt = (char*)malloc(strlen(prompt)+1);
	memset(*pPrompt, 0, strlen(prompt)+1);
    strcpy(*pPrompt, prompt);
}

void ChangePromptCommand::execute() {
	setPrompt(getArg(1)); //will set to NULL if no parameters passed
}
				
ShowPidCommand::ShowPidCommand(const char* cmd_line, pid_t pid) :
				BuiltInCommand(cmd_line), pid(pid) {}

void ShowPidCommand::execute() {
	std::cout << "smash pid is " << getPid() << endl;
}

GetCurrDirCommand::GetCurrDirCommand(const char* cmd_line) : 
				BuiltInCommand(cmd_line) {}

void GetCurrDirCommand::execute() {
	char* cwd = get_current_dir_name();
	if (cwd == nullptr) {
		perror("smash error: get_current_dir_name failed");
		return;
	}
	std::cout << cwd << std::endl;
	free(cwd);
}

ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) :
				BuiltInCommand(cmd_line), plastPwd(plastPwd) {}
				
void ChangeDirCommand::setLastPwd(const char* wd) { //free and realloc lastPwd (private data member of SmallShell class)
	free(*plastPwd);
	*plastPwd = (char*)malloc(strlen(wd)+1);
	memset(*plastPwd, 0, strlen(wd)+1);
    strcpy(*plastPwd, wd);
}

void ChangeDirCommand::execute() {
    int argc = getArgCount();
	if (argc == 1) { return; }
	if (argc > 2) {
		std::cerr << "smash error: cd: too many arguments" << std::endl;
		return;
	}
	const char* newPwd = getArg(1);
	if (strcmp(newPwd,"-") == 0) {
		newPwd = getLastPwd();
		if (newPwd == nullptr) {
			std::cerr << "smash error: cd: OLDPWD not set" << std::endl;
			return;
		}
	}
	char* cwd = get_current_dir_name();
	if (cwd == nullptr) {
		perror("smash error: get_current_dir_name failed");
		return;
	}
	if (chdir(newPwd) == -1) {
		perror("smash error: chdir failed");
		free(cwd);
		return;
	}
	setLastPwd(cwd);
	free(cwd);
}

JobsCommand::JobsCommand(const char* cmd_line, JobsList* jobs) : 
				BuiltInCommand(cmd_line), jobs(jobs) {}

void JobsCommand::execute() { 
	jobs->printJobsList();  
}

KillCommand::KillCommand(const char* cmd_line, JobsList* jobs) : 
				BuiltInCommand(cmd_line), jobs(jobs) {}

void KillCommand::execute() { 
	int signum = 0;
	int jobId = 0;
	if (getArgCount() != 3 || getArg(1)[0] != '-') {
		std::cerr << "smash error: kill: invalid arguments" << std::endl;
		return;
	}
    try {
        signum = (-1)*std::stoi(getArg(1)); //extract sig_num
    } catch (const std::invalid_argument& ia) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }
    try {
        jobId = std::stoi(getArg(2));
    } catch (const std::invalid_argument& ia) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }
    JobsList::JobEntry* j = jobs->getJobById(jobId);
    if (!j) {
        std::cerr << "smash error: kill: job-id " << jobId << " does not exist" << std::endl;
        return;
    }
	/*try {
		signum = (-1)*std::stoi(getArg(1)); //extract sig_num
	} catch (const std::invalid_argument& ia) {
		signum = -1;
	}
    if (signum == -1 || signum > 31) {
        std::cerr << "smash error: kill: invalid arguments" << std::endl;
        return;
    }*/
	pid_t pid = j->getPid();
	if (kill((-1)*pid, signum) == -1) {
		perror("smash error: kill failed");
		return;
	}
	std::cout << "signal number " << signum << " was sent to pid " << pid << std::endl;
}

ForegroundCommand::ForegroundCommand(const char* cmd_line, JobsList* jobs) : 
				BuiltInCommand(cmd_line), jobs(jobs) {}
				
void ForegroundCommand::execute() {
	int argc = getArgCount();
	JobsList::JobEntry *j = nullptr;
	int jobId = 0;
	if (argc > 2) {
		std::cerr << "smash error: fg: invalid arguments" << std::endl;
		return;
	}
	if (argc == 1) {
		j = jobs->getLastJob(&jobId);
		if (j == nullptr) {
			std::cerr << "smash error: fg: jobs list is empty" << std::endl;
			return;
		}
	}
	if (argc == 2) {
		try {
			jobId = std::stoi(getArg(1));
		} catch (const std::invalid_argument& ia) {
            std::cerr << "smash error: fg: invalid arguments" << std::endl;
            return;
		}
		j = jobs->getJobById(jobId);
		if (j == nullptr) {
		std::cerr << "smash error: fg: job-id " << jobId << " does not exist" << std::endl;
		return;
		}
	}
	pid_t pid = j->getPid();
	std::cout << j->getCommandLine() << " : " << pid << std::endl;	
	if (kill((-1)*pid, SIGCONT) == -1) {
		perror("smash error: kill failed");
		return;
	}
	j->resume();
    jobs->setFgCommand(pid, j->getCommandLine().c_str());
    int status = 0;
	if (waitpid(pid, &status, WUNTRACED) == -1) {
        perror("smash error: waitpid failed");
        return;
    }
	if (WIFEXITED(status)) { jobs->removeJob(pid); }
}

BackgroundCommand::BackgroundCommand(const char* cmd_line, JobsList* jobs) : 
				BuiltInCommand(cmd_line), jobs(jobs) {}
				
void BackgroundCommand::execute() {
	int argc = getArgCount();
	JobsList::JobEntry *j = nullptr;
	int jobId = 0;
	if (argc > 2) {
		std::cerr << "smash error: bg: invalid arguments" << std::endl;
		return;
	}
	if (argc == 1) {
		j = jobs->getLastStoppedJob(&jobId);
		if (j == nullptr) {
			std::cerr << "smash error: bg: there is no stopped jobs to resume" << std::endl;
			return;
		}
	}
	if (argc == 2) {
		try {
			jobId = std::stoi(getArg(1));
		} catch (const std::invalid_argument& ia) {
			jobId = -1;
		}
		/*if (jobId < 1) {
			std::cerr << "smash error: bg: invalid arguments" << std::endl;
			return;
		}*/
		j = jobs->getJobById(jobId);
		if (j == nullptr) {
		std::cerr << "smash error: bg: job-id " << jobId << " does not exist" << std::endl;
		return;
		}
		if (!j->isStopped()) {
			std::cerr << "smash error: bg: job-id " << jobId << " is already running in the background" << std::endl;
			return;
		}
	}
	pid_t pid = j->getPid();
	std::cout << j->getCommandLine() << " : " << pid << std::endl;
	if (kill((-1)*pid, SIGCONT) == -1) {
		perror("smash error: kill failed");
		return;
	}
	j->resume();
}

QuitCommand::QuitCommand(const char* cmd_line, JobsList* jobs) : 
				BuiltInCommand(cmd_line), jobs(jobs) {}
				
void QuitCommand::execute() {
    for (int i = 1; getArg(i) != nullptr; ++i) {
        if (strcmp(getArg(i), "kill") == 0) {
            jobs->killAllJobs();
            break;
        }
    }
    exit(0);
}

CopyCommand::CopyCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {}

void CopyCommand::execute() {
    if (getArgCount() != 3) {
        std::cerr << "smash error: cp: invalid arguments" << std::endl;
        exit(0);
    }
    int oldFileFd = -1;
    int newFileFd = -1;
    oldFileFd = open(getArg(1), O_RDONLY, 0666);
    if(oldFileFd == -1) {
        perror("smash error: open failed");
        exit(0);
    }
    char* resolvedSrcPath = realpath(getArg(1), nullptr);
    if (resolvedSrcPath == nullptr) {
        perror("smash error: realpath failed");
        exit(0);
    }
    char* resolvedDstPath = realpath(getArg(2), nullptr);
    if (resolvedDstPath == nullptr && errno != ENOENT) {
        perror("smash error: realpath failed");
        exit(0);
    }
    if (resolvedDstPath != nullptr && strcmp(resolvedSrcPath,resolvedDstPath) == 0) {
        std::cout << "smash: " << getArg(1) << " was copied to " << getArg(2) << endl;
        exit(0);
    }
    free(resolvedSrcPath);
    free(resolvedDstPath);
    newFileFd = open(getArg(2), O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if(newFileFd == -1) {
        perror("smash error: open failed");
        exit(0);
    }
    char* buf[BUFFER_SIZE];
    int bytesToCopy = -1;
    while (bytesToCopy != 0) { //EOF reached
        bytesToCopy = read(oldFileFd, buf, BUFFER_SIZE);
        if (bytesToCopy == -1) {
            perror("smash error: read failed");
            exit(0);
        }
        int bytesCopied = 0;
        while (bytesToCopy != bytesCopied) {
            int val = write(newFileFd, buf+bytesCopied, bytesToCopy-bytesCopied);
            if (val == -1) {
                perror("smash error: write failed");
                exit(0);
            }
            bytesCopied += val;
        }
    }
    std::cout << "smash: " << getArg(1) << " was copied to " << getArg(2) << endl;
    if (close(oldFileFd) == -1) {
        perror("smash error: close failed");
        exit(0);
    }
    if (close(newFileFd) == -1) {
        perror("smash error: close failed");
        exit(0);
    }
    exit(0);
}

ExternalCommand::ExternalCommand(const char* cmd_line) :
        Command(cmd_line) {}

RedirectionCommand::RedirectionCommand(const char *cmd_line) :
        Command(cmd_line), append(true), filename(), stdout_fd(-1) {
    char cmd_c[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_c, cmd_line);
    _removeBackgroundSign(cmd_c);
    std::string cmd_s = _trim(string(cmd_c))+" ";
    size_t index = cmd_s.find_first_of('>');
    if (cmd_s.at(index + 1) == '>') { //>> command
        index++;
    } else { //> command
        append = false;
    }
    filename = _trim(std::string(cmd_s.substr(index + 1)));
}

bool RedirectionCommand::prepare() {
    stdout_fd = dup(1);
    if (stdout_fd == -1) {
        perror("smash error: dup failed");
        return false;
    }
    if ((close(1)) == -1) {
        perror("smash error: close failed");
        return false;
    }
    if (append) {
        if(open(filename.c_str(), O_WRONLY|O_CREAT|O_APPEND, 0666) == -1) {
            perror("smash error: open failed");
            return false;
        }
    } else {
        if(open(filename.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0666) == -1) {
            perror("smash error: open failed");
            return false;
        }
    }
    return true;
}

void RedirectionCommand::cleanup() {
    if ((close(1)) == -1) {
        perror("smash error: close failed");
    }
    if (dup2(stdout_fd,1) == -1) {
        perror("smash error: dup2 failed");
        return;
    }
}

PipeCommand::PipeCommand(const char *cmd_line) : Command(cmd_line), outputToStderr(true), lCommandLine(), rCommandLine() {
    char cmd_c[COMMAND_ARGS_MAX_LENGTH];
    strcpy(cmd_c, cmd_line);
    _removeBackgroundSign(cmd_c);
    std::string cmd_s = _trim(string(cmd_c))+" ";
    size_t index = cmd_s.find_first_of('|');
    lCommandLine = _trim(std::string(cmd_s.substr(0, index)));
    if (cmd_s.at(index + 1) == '&') { // |& command
        index++;
    } else { // | command
        outputToStderr = false;
    }
    rCommandLine = _trim(std::string(cmd_s.substr(index + 1)));
}

void PipeCommand::execute() {
    int fd[2];
    pipe(fd);
    SmallShell& smash = SmallShell::getInstance();
    Command* lCommand = smash.CreateCommand(lCommandLine.c_str());
    pid_t pid = fork();
    if (pid == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (pid == 0) { //left child
        /*if (setpgrp() == -1) {
            perror("smash error: setpgrp failed");
            exit(0);
        }*/
        int output_fd = outputToStderr ? 2 : 1 ;
        if (dup2(fd[1],output_fd) == -1) {
            perror("smash error: dup2 failed");
            exit(0);
        }
        if (close(fd[0]) == -1) {
            perror("smash error: close failed");
            exit(0);
        }
        if (close(fd[1]) == -1) {
            perror("smash error: close failed");
            exit(0);
        }
        lCommand->execute();
        exit(0);
    } else { //father
        Command* rCommand = smash.CreateCommand(rCommandLine.c_str());
        pid =  fork();
        if (pid == -1) {
            perror("smash error: fork failed");
            return;
        }
        if (pid == 0) { //right child
            /*if (setpgrp() == -1) {
                perror("smash error: setpgrp failed");
                exit(0);
            }*/
            if (dup2(fd[0],0) == -1) {
                perror("smash error: dup2 failed");
                exit(0);
            }
            if (close(fd[0]) == -1) {
                perror("smash error: close failed");
                exit(0);
            }
            if (close(fd[1]) == -1) {
                perror("smash error: close failed");
                exit(0);
            }
            rCommand->execute();
            exit(0);
        } else { //father
            if (close(fd[0]) == -1) {
                perror("smash error: close failed");
            }
            if (close(fd[1]) == -1) {
                perror("smash error: close failed");
            }
            if (wait(nullptr) == -1) {
                perror("smash error: wait failed");
            }
            if (wait(nullptr) == -1) {
                perror("smash error: wait failed");
            }
            exit(0);
        }
    }
}


void ExternalCommand::execute() {
    char arg0[10];
    char arg1[3];
    char arg2[COMMAND_ARGS_MAX_LENGTH];
    strcpy(arg0, "/bin/bash");
    strcpy(arg1, "-c");
    strcpy(arg2, getCommandLine().c_str());
    _removeBackgroundSign(arg2);
    char* bashArgs[4] = {arg0, arg1, arg2, nullptr};
    if (execv("/bin/bash", bashArgs) == -1) {
        perror("smash error: execv failed");
        exit(0);
    }
}


TimeoutCommand::TimeoutCommand(const char *cmd_line) : Command(cmd_line) {}

void TimeoutCommand::execute() {
    if (getArgCount() < 3) {
        std::cerr << "smash error: timeout: invalid arguments" << std::endl;
        return;
    }
    int timeout = 0;
    try {
        timeout = std::stoi(getArg(1));
    } catch (const std::invalid_argument& ia) {
        std::cerr << "smash error: timeout: invalid arguments" << std::endl;
        return;
    }
    if (timeout <= 0) {
        std::cerr << "smash error: timeout: invalid arguments" << std::endl;
        return;
    }
    SmallShell& smash = SmallShell::getInstance();
    smash.setTimeoutDuration(timeout);
    std::string commandLine = getCommandLine();
    smash.setTimeoutOriginalCommandLine(commandLine); //with "timeout"
    size_t index = commandLine.find_first_of(WHITESPACE);
    commandLine = _trim(commandLine.substr(index)); //remove 1st arg ("timeout")
    index = commandLine.find_first_of(WHITESPACE);
    commandLine = _trim(commandLine.substr(index)); //remove 2nd arg ("<timeout duration>")
    smash.executeCommand(commandLine.c_str());
}


void JobsList::printJobsList() {
    removeFinishedJobs();
    for (const JobEntry& j : jobsList) {
        std::cout << "[" << j.getJobId() << "] " <<
                  j.getCommandLine() << " : " <<
                  j.getPid() << " " <<
                  j.getSecondsElapsed() << " secs";
        if (j.isStopped()) {
            std::cout << " (stopped)";
        }
        std::cout << endl;
    }
}

void JobsList::killAllJobs() { // (print format in pdf p.9)
    std::cout << "smash: sending SIGKILL signal to " << jobsList.size() << " jobs:" << endl;
    for (const JobEntry &j : jobsList) {
        std::cout << j.getPid() << ": " << j.getCommandLine() << endl;
    }
    for (const JobEntry &j : jobsList) {
        if (kill((-1)*j.getPid(), SIGKILL) == -1) {
            perror("smash error: kill failed");
            return;
        }
    }
    jobsList.clear();
}

void JobsList::removeFinishedJobs() {
    std::list<int> toRemove;
    for (JobEntry &j : jobsList) {
        pid_t pid = (waitpid(j.getPid(), nullptr, WNOHANG));
        if (pid == -1) {
            perror("smash error: waitpid failed");
        }
        if (pid > 0) {
            toRemove.push_back(pid);
        }
    }
    for (int i : toRemove) {
        removeJob(i);
    }
}

void JobsList::addJob(const std::string CommandLine, pid_t pid, bool isStopped) {
    removeFinishedJobs();
    time_t insertionTime = time(nullptr);
    if (insertionTime == -1) {
        perror("smash error: time failed");
    }
    int jobId = 1;
    if (!jobsList.empty()) {
        jobId = jobsList.back().getJobId() + 1;
    }
    JobEntry j = JobEntry(pid, jobId, CommandLine, isStopped, insertionTime);
    jobsList.push_back(j);
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    if (jobsList.empty()) {
        return nullptr;
    }
    *lastJobId = jobsList.back().getJobId();
    return &jobsList.back();
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    for(std::list<JobEntry>::reverse_iterator it = jobsList.rbegin(); it != jobsList.rend(); it++) {
        if (it->isStopped()) {
            *jobId = it->getJobId();
            return &(*it);
        }
    }
    return nullptr;
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for(std::list<JobEntry>::reverse_iterator it = jobsList.rbegin(); it != jobsList.rend(); it++) {
        if (it->getJobId() == jobId) {
            return &(*it);
        }
    }
    return nullptr;
}

JobsList::JobEntry *JobsList::getJobByPid(int jobPid) {
    for(std::list<JobEntry>::reverse_iterator it = jobsList.rbegin(); it != jobsList.rend(); it++) {
        if (it->getPid() == jobPid) {
            return &(*it);
        }
    }
    return nullptr;
}

JobsList::JobEntry::JobEntry(pid_t pid, int jobId, std::string cmd_line, bool stopped, time_t time) :
        pid(pid), jobId(jobId), cmd_line(cmd_line), stopped(stopped), insertionTime(time) {
}

double JobsList::JobEntry::getSecondsElapsed() const {
    time_t currentTime = time(nullptr);
    if (currentTime == -1) {
        perror("smash error: time failed");
    }
    return difftime(currentTime, insertionTime);
}

void JobsList::setFgCommand(pid_t pid, const char *cmd_line) {
    fgCommandLine = std::string(cmd_line);
    fgPid = pid;
}

bool operator==(const JobsList::JobEntry & j1, const JobsList::JobEntry & j2) {
    return j1.getJobId() == j2.getJobId();
}

void JobsList::JobEntry::resetSecondsElapsed() {
    time_t currentTime = time(nullptr);
    if (currentTime == -1) {
        perror("smash error: time failed");
    } else { insertionTime = currentTime; }
}

void JobsList::removeJob(pid_t pid) {
    jobsList.remove(*(getJobByPid(pid)));
}

SmallShell::SmallShell() : redirectionCommand(nullptr), forkCommand(false), prompt(nullptr), lastPwd(nullptr), smashPid(0) {
    smashPid = getpid();
}

SmallShell::~SmallShell() {
    delete(redirectionCommand);
	free(prompt);
	free(lastPwd);
}

Command * SmallShell::CreateCommand(const char* cmd_line) {
    std::string cmd_s = (_trim(string(cmd_line)));
    size_t special_index = cmd_s.find_first_of('>');
    if (special_index != string::npos) {   //redirection
        RedirectionCommand* r = new RedirectionCommand(cmd_line);
        setRedirectionCommand(r);
        cmd_s = cmd_s.substr(0, special_index);
    }
    special_index = cmd_s.find_first_of('|');
    if (special_index != string::npos) {   //pipe
        forkCommand = true;
        return new PipeCommand(cmd_line);
    }
    std::string first_arg(cmd_s);
    size_t first_arg_length = first_arg.find_first_of(WHITESPACE+'&');
    if (first_arg.length() == 0) { return nullptr; } //if nothing or only whitespace is entered
    if (first_arg_length != string::npos) { //if args
        first_arg = first_arg.substr(0, first_arg_length);
    }
    if (first_arg == "timeout") {
        return new TimeoutCommand(cmd_line);
    }
	if (first_arg == "chprompt") {
		return new ChangePromptCommand(cmd_s.c_str(), getPromptPtr());
	}
	if (first_arg == "showpid") {
		return new ShowPidCommand(cmd_s.c_str(), smashPid);
	}
	if (first_arg == "pwd") {
		return new GetCurrDirCommand(cmd_s.c_str());
	}
	if (first_arg == "cd") {
		return new ChangeDirCommand(cmd_s.c_str(), getLastPwdPtr());
	}
	if (first_arg == "jobs") {
		return new JobsCommand(cmd_s.c_str(), getJobsListPtr());
	}
	if (first_arg == "kill") {
		return new KillCommand(cmd_s.c_str(), getJobsListPtr());
	}
	if (first_arg == "fg") {
		return new ForegroundCommand(cmd_s.c_str(), getJobsListPtr());
	}
	if (first_arg == "bg") {
		return new BackgroundCommand(cmd_s.c_str(), getJobsListPtr());
	}
	if (first_arg == "quit") {
		return new QuitCommand(cmd_s.c_str(), getJobsListPtr());
	}
    forkCommand = true;
    if (first_arg == "cp") {

        return new CopyCommand(cmd_s.c_str());
    }
    return new ExternalCommand(cmd_s.c_str());
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    if (!cmd) { return; } //if nothing or only whitespace is entered
    jobsList.removeFinishedJobs();
    if (forkCommand) {
        forkCommand = false;
	   	pid_t pid = fork();
		if (pid == -1) {
			perror("smash error: fork failed");
            clearRedirectionCommand();
            jobsList.clearFgCommand();
            delete cmd;
			return;
		}

		if (pid == 0) { //child
			if (setpgrp() == -1) {
                perror("smash error: setpgrp failed");
                clearRedirectionCommand();
                jobsList.clearFgCommand();
                delete cmd;
                exit(0);
			}
            if (redirectionCommand) {
                if (redirectionCommand->prepare()) { cmd->execute(); }
            } else { cmd->execute(); }
			exit(0);
		} else { //father
		    if (getTimeoutDuration() > 0) {
		        TimeoutEntry t(pid, getTimeoutDuration(), getTimeoutOriginalCommandLine());
		        timedCommands.push_back(t);
		        manageAlarm();
		    }
			if (_isBackgroundComamnd(cmd_line)) { //background
                if (getTimeoutDuration() > 0) {
                    jobsList.addJob(getTimeoutOriginalCommandLine(), pid);
                } else {
                    jobsList.addJob(cmd_line, pid);
                }
			} else { //foreground
                if (getTimeoutDuration() > 0) {
                    jobsList.setFgCommand(pid, getTimeoutOriginalCommandLine().c_str());
                } else {
                    jobsList.setFgCommand(pid, cmd_line);
                }
			    if (waitpid(pid, nullptr, WUNTRACED) == -1) {
                    perror("smash error: waitpid failed");
                    clearRedirectionCommand();
                    jobsList.clearFgCommand();
                    delete cmd;
                    return;
                }
			}
		}
	} else { //no fork
        if (redirectionCommand) {
            if (redirectionCommand->prepare()) { cmd->execute(); }
            redirectionCommand->cleanup();
        } else { cmd->execute(); }
        if (getTimeoutDuration() > 0) {
            TimeoutEntry t(0, getTimeoutDuration(), getTimeoutOriginalCommandLine());
            timedCommands.push_back(t);
            manageAlarm();
        }
    }
    setTimeoutDuration(0);
    clearRedirectionCommand();
    jobsList.clearFgCommand();
    delete cmd;
}

void SmallShell::setRedirectionCommand(RedirectionCommand* redirectionCommand) {
    this->redirectionCommand = redirectionCommand;
}

void SmallShell::clearRedirectionCommand() {
    delete(redirectionCommand);
    redirectionCommand =  nullptr;
}

bool SmallShell::TimeoutEntry::sortByTimeToAlarm::operator()(const SmallShell::TimeoutEntry &t1,
                                                             const SmallShell::TimeoutEntry &t2) const {
    time_t currentTime = time(nullptr);
    if (currentTime == -1) {
        perror("smash error: time failed");
    }
    double t1TimeToAlarm = t1.timeoutDuration - (difftime(currentTime,t1.timestamp));
    double t2TimeToAlarm = t2.timeoutDuration - (difftime(currentTime,t2.timestamp));
    return (t1TimeToAlarm <= t2TimeToAlarm);
}

void SmallShell::manageAlarm() {  //sort by timeout duration - ( current time - timestamp )
    TimeoutEntry& t = timedCommands.back();
    timedCommands.sort(TimeoutEntry::sortByTimeToAlarm());
    if (timedCommands.size() == 1 || &t == &timedCommands.front()) {
        alarm(timeoutDuration);
    }
}

SmallShell::TimeoutEntry SmallShell::popTimedoutEntry() {
    TimeoutEntry t(timedCommands.front());
    timedCommands.pop_front();
    return t;
}

SmallShell::TimeoutEntry::TimeoutEntry(pid_t pid, int timeoutDuration, std::string commandLine)  : pid(pid), timeoutDuration(timeoutDuration), commandLine(commandLine) {
    timestamp = time(nullptr);
    if (timestamp == -1) {
        perror("smash error: time failed");
    }
}

void SmallShell::setNewAlarm() const {
    if (!timedCommands.empty()) {
        timedCommands.front().setAlarm();
    }
}

void SmallShell::TimeoutEntry::setAlarm() const {
    time_t currentTime = time(nullptr);
    if (currentTime == -1) {
        perror("smash error: time failed");
    }
    alarm(int((timeoutDuration - (difftime(currentTime,timestamp)))));
}

