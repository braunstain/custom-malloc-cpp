#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <map>

using namespace std;

#define COMMAND_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)

class Command {
protected:
    const char *cmd_line{};
public:
    explicit Command(const char *cmd_line);

    virtual ~Command() = default;

    virtual void execute() = 0;

    int extractArgs(const char* cmd_line, char **args);

    void cleanArgs(char **args, int num_of_args);

    const char *getCmdLine() const;

    //virtual void prepare();
    //virtual void cleanup();
    // TODO: Add your extra methods if needed
};

class BuiltInCommand : public Command {
public:
    explicit BuiltInCommand(const char *cmd_line);

    virtual ~BuiltInCommand() = default;
};

class ExternalCommand : public Command {
public:
    explicit ExternalCommand(const char *cmd_line);

    virtual ~ExternalCommand() {
    }

    void execute() override;
};

class PipeCommand : public Command {
    std::string m_cmd1;          // The first command string
    std::string m_cmd2;          // The second command string
    bool b_is_err_pipe;       // Indicates if the pipe is for stderr
public:
    PipeCommand(const char *cmd_1, const char* cmd_2, bool is_err);

    virtual ~PipeCommand() {
    }

    void execute() override;
};

class RedirectionCommand : public Command {
    // TODO: Add your data members
public:
    explicit RedirectionCommand(const char *cmd_line);

    virtual ~RedirectionCommand() {
    }

    void execute() override;
};

class ChangPromptCommand : public BuiltInCommand {
public:
    explicit ChangPromptCommand(const char *cmd_line);

    virtual ~ChangPromptCommand() = default;

    void execute() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmd_line, char **plastPwd);

    virtual ~ChangeDirCommand() {
    }

    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
public:
    explicit GetCurrDirCommand(const char *cmd_line);

    virtual ~GetCurrDirCommand() = default;

    void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
public:
    ShowPidCommand(const char *cmd_line);

    virtual ~ShowPidCommand() {
    }

    void execute() override;
};

class JobsList;

class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(const char *cmd_line);

    virtual ~QuitCommand() = default;

    void execute() override;
};


class JobsList {

    class JobEntry {

    public:
        int job_id;
        pid_t job_pid;
        bool isStopped;
        string cmd_line;

        JobEntry(string command, pid_t pid);
        ~JobEntry() = default;
    };

public:
    vector<JobEntry *> jobs;
    int max_job_id;

    JobsList();

    ~JobsList() {
        for (auto entry : jobs) {
            delete entry;
        }
    }

    void addJob(string command, pid_t pid);

    void printJobsList();

    void killAllJobs();

    void removeFinishedJobs();

    JobEntry *getJobById(int jobId);

    void removeJobById(int jobId);

    JobEntry *getLastJob(int *lastJobId);

    JobEntry *getLastStoppedJob(int *jobId);

    void updateMaxJobId();

};

class JobsCommand : public BuiltInCommand {
public:
    explicit JobsCommand(const char *cmd_line);

    virtual ~JobsCommand() = default;

    void execute() override;
};

class KillCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    KillCommand(const char *cmd_line);

    virtual ~KillCommand() = default;

    void execute() override;
};

class ForegroundCommand : public BuiltInCommand {
    // TODO: Add your data members
public:
    ForegroundCommand(const char *cmd_line);

    virtual ~ForegroundCommand() {
    }

    void execute() override;
};

class ListDirCommand : public Command {
public:
    ListDirCommand(const char *cmd_line);
    void printDirRec(const char* path, int space);

    virtual ~ListDirCommand() {
    }

    void execute() override;
};

class WhoAmICommand : public Command {
public:
    WhoAmICommand(const char *cmd_line);

    virtual ~WhoAmICommand() {
    }

    void execute() override;
};

class NetInfo : public Command {
    // TODO: Add your data members
public:
    NetInfo(const char *cmd_line);
    string getInfo(const char* cmd);
    string cidr_to_subnet(int cidr);
    string trimNewline(string s);

    virtual ~NetInfo() {
    }

    void execute() override;
};

class aliasCommand : public BuiltInCommand {
public:
    aliasCommand(const char *cmd_line);

    virtual ~aliasCommand() {
    }

    void execute() override;
};

class unaliasCommand : public BuiltInCommand {
public:
    unaliasCommand(const char *cmd_line);

    virtual ~unaliasCommand() {
    }

    void execute() override;
};


class SmallShell {
    SmallShell();
    static string prompt;
    static pid_t pid;
    char* last_working_directory;
    map<string, string> aliases;
    pid_t last_fg_command_pid;

public:
    static JobsList job_list;

    Command* CreateCommand(const char* cmd_line);

    SmallShell(SmallShell const&) = delete; // disable copy ctor
    void operator=(SmallShell const&) = delete; // disable = operator
    static SmallShell& getInstance() // make SmallShell singleton
    {
        static SmallShell instance; // Guaranteed to be destroyed.
        // Instantiated on first use.
        return instance;
    }

    ~SmallShell();

    void executeCommand(const char* cmd_line);

    string getPrompt() const {
        return prompt;
    }

    void setPrompt(const string& new_prompt) {
        prompt = new_prompt;
    };

    char* getLastWorkingDirectory() const {
        return last_working_directory;
    }

    void setLastWorkingDirectory(char* dir);

    void addAlias(const string& name, const string& command);
    void removeAlias(const string& name);
    map<string, string> getAliasList() const;
    string getAlias(const string& name) const;
    pid_t getLastFgCommandPid() const;
    void setLastFgCommandPid(pid_t pid);
};


#endif //SMASH_COMMAND_H_
