#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <string>
#include <list>

#define COMMAND_ARGS_MAX_LENGTH (200) // pdf says 80 characters
#define COMMAND_MAX_ARGS (20)
#define BUFFER_SIZE (4096)

class Command {
	const std::string cmd_line;
	int argc;
	char* argv[COMMAND_MAX_ARGS+1];
public:
    explicit Command(const char* cmd_line);
    virtual ~Command();
    virtual void execute() = 0;
    std::string getCommandLine() const { return cmd_line; }
protected:
    int getArgCount() const { return argc; }
    const char* getArg(int argNumber) const { return argv[argNumber]; }
};

class BuiltInCommand : public Command {
 public:
  explicit BuiltInCommand(const char* cmd_line);
  ~BuiltInCommand() override = default;
};

class ExternalCommand : public Command {
 public:
  explicit ExternalCommand(const char* cmd_line);
  ~ExternalCommand() override = default;
  void execute() override;
};

class PipeCommand : public Command {
    bool outputToStderr;
    std::string lCommandLine;
    std::string rCommandLine;
public:
    explicit PipeCommand(const char* cmd_line);
    ~PipeCommand() override = default;
    void execute() override;
};

class RedirectionCommand : public Command {
    bool append;
    std::string filename;
    int stdout_fd;
public:
    explicit RedirectionCommand(const char* cmd_line);
    ~RedirectionCommand() override = default;
    void execute() override {}
    bool prepare();
    void cleanup();
};

class ChangePromptCommand : public BuiltInCommand {
	char** pPrompt;
public:
    ChangePromptCommand(const char* cmd_line, char** pPrompt);
    ~ChangePromptCommand() override = default;
    void execute() override;
    void setPrompt(const char* prompt);
};

class ShowPidCommand : public BuiltInCommand {
    pid_t pid;
public:
    explicit ShowPidCommand(const char* cmd_line, pid_t pid);
    ~ShowPidCommand() override = default;
    void execute() override;
    pid_t getPid() const { return pid; }
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char* cmd_line);
    ~GetCurrDirCommand() override = default;
    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
    char** plastPwd;
public:
    ChangeDirCommand(const char* cmd_line, char** plastPwd);
    ~ChangeDirCommand() override = default;
    void execute() override;
    const char* getLastPwd() { return *plastPwd; }
    void setLastPwd(const char* wd);
};

class JobsList { //job-id sorted
public:
    class JobEntry {
	    pid_t pid;
	    int jobId;
        const std::string cmd_line;
	    bool stopped;
        time_t insertionTime;
	public:
        JobEntry(pid_t pid, int jobId, std::string cmd_line, bool stopped, time_t time);
        ~JobEntry() = default;
		pid_t getPid() const { return pid; }
		int getJobId() const { return jobId; }
		bool isStopped() const { return stopped; }
		void stop() { stopped = true; }
		void resume() { stopped = false; }
		double getSecondsElapsed() const;
        void resetSecondsElapsed();
        std::string getCommandLine() const { return cmd_line; }
        friend bool operator==(const JobEntry&, const JobEntry&);
	};
private:
    std::list<JobEntry> jobsList;
    pid_t fgPid; //0 if no job
    std::string fgCommandLine; //"" if no job
public:
    JobsList() = default;
    ~JobsList() = default;
    void printJobsList();
    void killAllJobs();
    void removeJob(pid_t pid);
    void removeFinishedJobs();
    JobEntry * getJobById(int jobId);
    JobEntry * getJobByPid(int jobPid);
    JobEntry * getLastJob(int* lastJobId);
    JobEntry *getLastStoppedJob(int *jobId);
    void addJob(std::string CommandLine, pid_t pid, bool isStopped = false);
    pid_t getFgPid() const { return fgPid; }
    std::string getFgCommandLine() const { return fgCommandLine; }
    void setFgCommand(pid_t pid, const char* cmd_line);
    void clearFgCommand() { setFgCommand(0,""); }
};

class JobsCommand : public BuiltInCommand {
	JobsList* jobs;
public:
    JobsCommand(const char* cmd_line, JobsList* jobs);
    ~JobsCommand() override = default;
    void execute() override;
};

class KillCommand : public BuiltInCommand {
	JobsList* jobs;
public:
    KillCommand(const char* cmd_line, JobsList* jobs);
    ~KillCommand() override = default;
    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
   JobsList* jobs;
public:
    ForegroundCommand(const char* cmd_line, JobsList* jobs);
    ~ForegroundCommand() override = default;
    void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
   JobsList* jobs;
public:
    BackgroundCommand(const char* cmd_line, JobsList* jobs);
    ~BackgroundCommand() override = default;
    void execute() override;
};

class QuitCommand : public BuiltInCommand {
	JobsList* jobs;
public: 
    QuitCommand(const char* cmd_line, JobsList* jobs);
    ~QuitCommand() override = default;
    void execute() override;
};

// TODO: should it really inherit from BuiltInCommand ?
class CopyCommand : public BuiltInCommand {
public:
    explicit CopyCommand(const char* cmd_line);
    ~CopyCommand() override = default;
    void execute() override;
};

class TimeoutCommand : public Command {
public:
    explicit TimeoutCommand(const char* cmd_line);
    ~TimeoutCommand() override = default;
    void execute() override;
};

class SmallShell {
public:
    class TimeoutEntry {
        pid_t pid;
        time_t timestamp;
        int timeoutDuration;
        std::string commandLine;
    public:
        TimeoutEntry(pid_t pid, int timeoutDuration, std::string commandLine);
        ~TimeoutEntry() = default;
        pid_t getPid() const { return pid; }
        time_t getTimestamp() const { return timestamp; }
        std::string getCommandLine() const { return commandLine; }
        void setAlarm() const;
        class sortByTimeToAlarm {
        public:
            sortByTimeToAlarm() = default;
            ~sortByTimeToAlarm() = default;
            bool operator()(const SmallShell::TimeoutEntry& t1, const SmallShell::TimeoutEntry& t2) const;
        };
    };
private:
    std::string timeoutOriginalCommandLine;
    std::list<TimeoutEntry> timedCommands;
    RedirectionCommand* redirectionCommand;
    int timeoutDuration;
    JobsList jobsList;
    bool forkCommand; //C'tor set this to false
    char* prompt;  //C'tor set this to NULL
    char* lastPwd; //C'tor set this to NULL
    pid_t smashPid;
    SmallShell();
public:
	~SmallShell(); //free lastPwd and prompt in D'tor
    Command *CreateCommand(const char* cmd_line);
    SmallShell(SmallShell const&)      = delete;
    void operator=(SmallShell const&)  = delete;
    static SmallShell& getInstance()
    {
      static SmallShell instance;
      return instance;
    }
    void executeCommand(const char* cmd_line);
    std::string getTimeoutOriginalCommandLine() { return timeoutOriginalCommandLine; }
    void setTimeoutOriginalCommandLine(std::string commandLine) { timeoutOriginalCommandLine = commandLine; }
    bool isPromptDefault() { return prompt == nullptr; }
    char** getPromptPtr() { return &prompt; }
    const char* getPrompt() { return prompt; }
    char** getLastPwdPtr() { return &lastPwd; }
    JobsList* getJobsListPtr() { return &jobsList; }
    void setRedirectionCommand(RedirectionCommand* redirectionCommand);
    void clearRedirectionCommand();
    void setTimeoutDuration(int duration) { timeoutDuration = duration; }
    int getTimeoutDuration() const { return timeoutDuration; }
    void manageAlarm();
    pid_t getPid() const { return smashPid; }
    TimeoutEntry popTimedoutEntry();
    void setNewAlarm() const;
};



#endif //SMASH_COMMAND_H_
