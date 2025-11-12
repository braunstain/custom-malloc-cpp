#include <unistd.h>
#include <string.h>
#include <iostream>
#include <utility>
#include <vector>
#include <regex>
#include <dirent.h>
#include <pwd.h>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <fcntl.h>
#include <fstream>
#include <array>
#include <limits.h>

using namespace std;

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cout << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cout << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

string _ltrim(const std::string &s) {
    size_t start = s.find_first_not_of(WHITESPACE);
    return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string &s) {
    size_t end = s.find_last_not_of(WHITESPACE);
    return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string &s) {
    return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char *cmd_line, char **args) {
    FUNC_ENTRY()
    int i = 0;
    std::istringstream iss(_trim(string(cmd_line)).c_str());
    for (std::string s; iss >> s;) {
        args[i] = (char *) malloc(s.length() + 1);
        memset(args[i], 0, s.length() + 1);
        strcpy(args[i], s.c_str());
        args[++i] = NULL;
    }
    return i;

    FUNC_EXIT()
}

bool _isBackgroundComamnd(const char *cmd_line) {
    const string str(cmd_line);
    return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char *cmd_line) {
    const string str(cmd_line);
    // find last character other than spaces
    unsigned int idx = str.find_last_not_of(WHITESPACE);
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

bool isComplexCommand(const char *cmd_line) {
    const string cmd = string(cmd_line);
    return cmd.find('*') == string::npos || cmd.find('?') == string::npos;
}

//////////////////////////////---------Command Class Implementation---------------////////////////////////////////////////////////

Command::Command(const char *cmd_line) {
    this->cmd_line = cmd_line;
}

BuiltInCommand::BuiltInCommand(const char *cmd_line): Command(cmd_line) {
    this->cmd_line = cmd_line;
}

int Command::extractArgs(const char* cmd_line, char **args) {
    int num_of_args = _parseCommandLine(cmd_line, args);
    return num_of_args;
}

void Command::cleanArgs(char **args, int num_of_args) {
    for (int i = 0; i < num_of_args; i++) {
        free(args[i]);
    }
}

ChangPromptCommand::ChangPromptCommand(const char *cmd_line): BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
ShowPidCommand::ShowPidCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
GetCurrDirCommand::GetCurrDirCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
ChangeDirCommand::ChangeDirCommand(const char* cmd_line, char** plastPwd) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
JobsCommand::JobsCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
ForegroundCommand::ForegroundCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
QuitCommand::QuitCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
KillCommand::KillCommand(const char *cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
aliasCommand::aliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}
unaliasCommand::unaliasCommand(const char* cmd_line) : BuiltInCommand(cmd_line) {
    this->cmd_line = cmd_line;
}

PipeCommand::PipeCommand(const char* cmd1, const char* cmd2, bool is_err_pipe) : Command(cmd1) {
    this->m_cmd1 = cmd1;
    this->m_cmd2 = cmd2;
    this->b_is_err_pipe = is_err_pipe;

}

ListDirCommand::ListDirCommand(const char* cmd_line) : Command(cmd_line) {
    this->cmd_line = cmd_line;
}
WhoAmICommand::WhoAmICommand(const char* cmd_line) : Command(cmd_line) {
    this->cmd_line = cmd_line;
}
NetInfo::NetInfo(const char* cmd_line) : Command(cmd_line) {
    this->cmd_line = cmd_line;
}

ExternalCommand::ExternalCommand(const char *cmd_line): Command(cmd_line) {
    this->cmd_line = cmd_line;
}

const char *Command::getCmdLine() const {
    return cmd_line;
}



//////////////////////////////---------SmallShell Class Implementation---------------////////////////////////////////////////////////


string SmallShell::prompt = "smash";
pid_t SmallShell::pid = getpid();
JobsList SmallShell::job_list;

SmallShell::SmallShell() {
    last_working_directory = nullptr;
    last_fg_command_pid = -1;
}

SmallShell::~SmallShell() {
    delete[] last_working_directory;
}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command *SmallShell::CreateCommand(const char *cmd_line) {
   string cmd_s = _trim(string(cmd_line));
   string firstWord = cmd_s.substr(0, cmd_s.find_first_of(" \n"));

   //checks for pipe
   size_t pipe_pos = cmd_s.find('|');
   if (pipe_pos != string::npos) {
       // is err pipe
       if (cmd_s[pipe_pos + 1] == '&') {
           string command1 = cmd_s.substr(0, pipe_pos);
           string command2 = cmd_s.substr(pipe_pos + 2);

           return new PipeCommand(command1.c_str(), command2.c_str(), true); // create PipeCommand for error pipe
       }
       // is regular pipe
       else {
           string command1 = cmd_s.substr(0, pipe_pos);
           string command2 = cmd_s.substr(pipe_pos + 1);

           return new PipeCommand(command1.c_str(), command2.c_str(), false); // Create PipeCommand for normal pipe
       }
   }
   if (firstWord == "chprompt") {
       return new ChangPromptCommand(cmd_line);
   }
   if (firstWord == "showpid") {
       return new ShowPidCommand(cmd_line);
   }
   if (firstWord == "pwd") {
       return new GetCurrDirCommand(cmd_line);
   }
   if (firstWord == "cd") {
       return new ChangeDirCommand(cmd_line, &last_working_directory);
   }
   if (firstWord == "jobs") {
       return new JobsCommand(cmd_line);
   }
   if (firstWord == "fg") {
       return new ForegroundCommand(cmd_line);
   }
   if (firstWord == "quit") {
       return new QuitCommand(cmd_line);
   }
   if (firstWord == "kill") {
       return new KillCommand(cmd_line);
   }
   if (firstWord == "alias") {
       return new aliasCommand(cmd_line);
   }
   if (firstWord == "unalias") {
       return new unaliasCommand(cmd_line);
   }
   if (getAlias(firstWord) != "") {
       size_t space_pos = cmd_s.find_first_of(" \n");
       if (space_pos != std::string::npos) {
           cmd_s = getAlias(firstWord) + cmd_s.substr(space_pos);
       }
       else {
           cmd_s = getAlias(firstWord);
       }
       const char* new_cmd = cmd_s.c_str();
       return CreateCommand(new_cmd);
   }
   if (firstWord == "listdir") {
       return new ListDirCommand(cmd_line);
   }
   if (firstWord == "whoami") {
       return new WhoAmICommand(cmd_line);
   }
   if (firstWord == "netinfo") {
       return new NetInfo(cmd_line);
   }
   return new ExternalCommand(cmd_line);
}

void SmallShell::executeCommand(const char *cmd_line) {
    job_list.removeFinishedJobs();

    //Searching for >>
    std::string cmd_s = _trim(std::string(cmd_line));
    std::string output_file;
    bool append = false;
    bool isRedir = false;

    size_t redir_pos = cmd_s.find_first_of(">");
    if (redir_pos != std::string::npos) {
        isRedir = true;
        // Append or overwrite
        if (cmd_s[redir_pos + 1] == '>') {
            append = true;
            output_file = cmd_s.substr(redir_pos + 2);
        }
        else
        {
            output_file = cmd_s.substr(redir_pos + 1);
        }
        output_file = _trim(output_file);
        cmd_s = cmd_s.substr(0, redir_pos);
        cmd_line = cmd_s.c_str();

    }
    Command* cmd = CreateCommand(cmd_line);

    if (isRedir) {
        int original_stdout = dup(STDOUT_FILENO); // Save stdout
        int fd;
        if (append) {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
        }
        else {
            fd = open(output_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if (fd < 0) {
            perror("smash error: open failed");
            delete cmd;
            return;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
        cmd->execute();

        dup2(original_stdout, STDOUT_FILENO);
        close(original_stdout);
    }
    else {
        cmd->execute(); // Execute normally
    }
    delete cmd;
}

void SmallShell::setLastWorkingDirectory(char* dir) {

    if (last_working_directory == nullptr)
    {
        last_working_directory = new char[PATH_MAX];
    }
    strcpy(last_working_directory, dir);
}
void SmallShell::addAlias(const string& name, const string& command) {
    aliases[name] = command;
    return;
};
void SmallShell::removeAlias(const string& name) {
    aliases.erase(name);
    return;
}
map<string, string> SmallShell::getAliasList() const {
    return aliases;
}
string SmallShell::getAlias(const string& name) const {
    auto it = aliases.find(name);
    if (it != aliases.end()) {
        return it->second;
    }
    return "";
}
pid_t SmallShell::getLastFgCommandPid() const {
    return last_fg_command_pid;
}
void SmallShell::setLastFgCommandPid(pid_t pid) {
    last_fg_command_pid = pid;
}



//////////////////////////////---------Built-In Commands---------------////////////////////////////////////////////////

//chprompt command will allow the user to change the prompt displayed by the smash while
//waiting for the next command.

void ChangPromptCommand::execute() {
    char *args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    SmallShell &shell = SmallShell::getInstance();

    if (num_of_args > 1) {
        shell.setPrompt(args[1]);
    } else {
        shell.setPrompt("smash");
    }

    cleanArgs(args, num_of_args);
}

// The showpid command prints the pid.

void ShowPidCommand::execute() {
    pid_t processID = getpid();
    std::cout << "smash pid is " << processID << std::endl;
}



// The pwd command prints the full path of the current working directory.

void GetCurrDirCommand::execute() {
    char cwd[PATH_MAX]; // Buffer to store the current working directory

    // Get the current working directory
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        cout << cwd << endl;
    } else {
        perror("getcwd has failed"); // Print error if getcwd fails
    }
}
// cd command changes dir

void ChangeDirCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    char cwd[PATH_MAX]; // Buffer to store the current working directory
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    if (num_of_args > 2)
    {
        std::cerr << "smash error: cd: too many arguments" << endl;
    }
    else if (getcwd(cwd, sizeof(cwd)) == nullptr) {
        perror("getcwd has failed");
    }
    else if (strcmp(args[1],"-") == 0)
    {
        if (shell.getLastWorkingDirectory() == nullptr)
        {
            std::cerr << "smash error: cd: OLDPWD not set" << endl;
        }
        else
        {
            if (chdir(shell.getLastWorkingDirectory()) != 0)
            {
                perror("chdir has failed");
            }
            else
            {
                shell.setLastWorkingDirectory(cwd);
            }
        }
    }
    else
    {
        if (chdir(args[1]) != 0)
        {
            perror("chdir has failed");
        }
        else
        {
            shell.setLastWorkingDirectory(cwd);
        }
    }
    cleanArgs(args, num_of_args);
}

// jobs command prints the jobs list which contains the unfinished jobs
void JobsCommand::execute() {
    SmallShell::job_list.printJobsList();
}

void ForegroundCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);
    SmallShell& shell = SmallShell::getInstance();
    if ((num_of_args == 2 && atoi(args[1]) < 1) || num_of_args > 2)
    {
        cerr << "smash error: fg: invalid arguments" << endl;
    }
    else if (num_of_args == 1)
    {
        const auto job = shell.job_list.getJobById(shell.job_list.max_job_id);
        if (job != nullptr)
        {
            pid_t pid = job->job_pid;
            cout << job->cmd_line << " " << pid << endl;
            shell.setLastFgCommandPid(pid);
            waitpid(pid, nullptr, 0);
        }
        else
        {
            cerr << "smash error: fg: jobs list is empty" << endl;
        }
    }
    else
    {
        const auto job = shell.job_list.getJobById(atoi(args[1]));
        if (job != nullptr)
        {
            pid_t pid = job->job_pid;
            cout << job->cmd_line << " " << pid << endl;
            shell.setLastFgCommandPid(pid);
            waitpid(pid, nullptr, 0);
        }
        else
        {
            cerr << "smash error: fg: job-id " << args[1] << " does not exist" << endl;
        }
    }
    cleanArgs(args, num_of_args);
}

// quit command exits the smash.
void QuitCommand::execute() {
    SmallShell &shell = SmallShell::getInstance();
    char *args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);

    if (num_of_args > 1 && strcmp(args[1], "kill") == 0) {
        cout << "smash: sending SIGKILL signal to " << shell.job_list.jobs.size() << " jobs:" << endl;
        shell.job_list.killAllJobs();
    }
    cleanArgs(args, num_of_args);
    exit(0);
}


// Kill command sends a signal whose number is specified by <signum> to a job whose
//sequence ID in jobs list is<job - id>
void KillCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    char *args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);

    if (num_of_args != 3 || args[1][0] != '-') {
        cerr << "smash error: kill: invalid arguments" << endl;
        return;
    }
    int signal = atoi(&args[1][1]);
    const int job_id = atoi(args[2]);
    const auto job = shell.job_list.getJobById(job_id);

    if (job == nullptr) {
        cerr << "smash error: kill: job-id " << job_id<< " does not exist" << endl;
        return;
    }

    int job_pid = job->job_pid;
    if (kill(job_pid, signal) == -1) {
        cout << "signal number " << signal << " was sent to pid " << job_pid << endl;
        perror("smash error: kill failed");
        return;
    }

    cout << "signal number " << signal << " was sent to pid " << job_pid << endl;
    cleanArgs(args, num_of_args);
}

void aliasCommand::execute()
{
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    SmallShell& shell = SmallShell::getInstance();
    if (num_of_args == 1)
    {
        //list aliases      
        for (const auto& alias : shell.getAliasList()) {
            std::cout << alias.first << "='" << alias.second << "'" << std::endl;
        }
    }
    else
    {
        //create
        string cmd_string(cmd_line);
        int pos = cmd_string.find_first_of("=");
        string alias = cmd_string.substr(6, pos - 6); //the alias starts after the word 'alias' so 6 position
        string command = cmd_string.substr(pos + 2, cmd_string.length() - pos - 3);
        regex aliasPattern("^alias [a-zA-Z0-9_]+='[^']*'$");
        if (alias == "chprompt" || alias == "showpid" || alias == "pwd" || alias == "cd" ||
            alias == "jobs" || alias == "fg" || alias == "quit" || alias == "kill" ||
            alias == "alias" || alias == "unalias" || shell.getAlias(alias) != "")
        {
            std::cerr << "smash error: alias: " << alias << " already exists or is a reserved command" << std::endl;
        }
        else if (!regex_match(cmd_line, aliasPattern))
        {
            std::cerr << "smash error: alias: invalid alias format" << std::endl;
        }
        else
        {
            shell.addAlias(alias, command);
        }
    }
    cleanArgs(args, num_of_args);
}

void unaliasCommand::execute()
{
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = _parseCommandLine(cmd_line, args);
    SmallShell& shell = SmallShell::getInstance();
    if (num_of_args == 1)
    {
        std::cerr << "smash error: unalias: not enough arguments" << std::endl;
    }
    else
    {
        for (int i = 1; i < num_of_args; i++)
        {
            if (shell.getAlias(args[i]) == "")
            {
                std::cerr << "smash error: unalias: " << args[i] << " alias does not exist" << std::endl;
                cleanArgs(args, num_of_args);
                return;
            }
            else
            {
                shell.removeAlias(args[i]);
            }
        }
    }
    cleanArgs(args, num_of_args);
}


//////////////////////////////---------External Commands---------------////////////////////////////////////////////////

void ExternalCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    string command = _trim(string(cmd_line));
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);



    // check if background command and remove background sign
    const bool is_back = _isBackgroundComamnd(cmd_line);


    char command_copy[COMMAND_MAX_LENGTH];
    strcpy(command_copy, cmd_line);
    if (is_back) {
        _removeBackgroundSign(command_copy);
    }

    //check if command is complex
    const bool is_complex = isComplexCommand(cmd_line);

    const pid_t pid = fork();
    if (pid == 0) { // child
        if (setpgrp() == -1) {
            perror("setpgrp has failed");
        }

        if (is_complex) { // for complex External Commands
            char bash[] = "/bin/bash";
            char flag[] = "-c";
            char *complex_exec_args[] = {bash, flag, command_copy, nullptr};
            execvp(bash, complex_exec_args);
            perror("execvp has failed");
        }else { // for simple External Commands
            execvp(args[0], args);
            perror("execvp has failed");
        }
    } else if (pid > 0) { // father
        if (is_back) { // Background Command
            SmallShell::job_list.addJob(command, pid);
        } else { // Foreground Command
            shell.setLastFgCommandPid(pid);
            waitpid(pid, nullptr, 0);
        }
            cleanArgs(args, num_of_args);
    } else {
        perror("smash error: fork failed");
    }
}


//////////////////////////////-----------Special Commands----------------/////////////////////////////////////////////////

void PipeCommand::execute() {
    SmallShell& shell = SmallShell::getInstance();
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("smash error: pipe failed");
        return;
    }
    pid_t pid1 = fork();
    if (pid1 == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (pid1 == 0) { //child
        close(pipefd[0]);
        if (b_is_err_pipe)
        {
            dup2(pipefd[1], 2); // pipefd1 <- STDERR
        }
        else
        {
            dup2(pipefd[1], 1); // pipefd1 <- STDOUT
        }
        Command* cmd1 = shell.CreateCommand(m_cmd1.c_str());
        cmd1->execute();
        close(pipefd[1]);
        exit(1);
    }
    waitpid(pid1, nullptr, 0); //father
    pid_t pid2 = fork();
    if (pid2 == -1) {
        perror("smash error: fork failed");
        return;
    }
    if (pid2 == 0) { //second child
        close(pipefd[1]);
        dup2(pipefd[0], 0); // pipefd0 <- STDIN
        Command* cmd2 = shell.CreateCommand(m_cmd2.c_str());
        cmd2->execute();
        close(pipefd[0]);
        exit(1);
    }
    else { //father
        close(pipefd[0]);
        close(pipefd[1]);
        waitpid(pid2, nullptr, 0);;
    }
}


void ListDirCommand::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);
    if (num_of_args > 2)
    {
        std::cerr << "smash error: listdir: too many arguments" << std::endl;
    }
    if (num_of_args == 1)
    {
        printDirRec(".", 0);
    }
    else
    {
        printDirRec(args[1], 0);
    }
    cleanArgs(args, num_of_args);
}
void ListDirCommand::printDirRec(const char* path, int space) {
    struct dirent** namelist;
    int n;
    n = scandir(path, &namelist, nullptr, alphasort);
    if (n < 0) {
        perror("smash error: scandir failed");
        return;
    }
    for (int i = 0; i < n; ++i) 
    {
        // Skip the "." and ".." entries
        if (strcmp(namelist[i]->d_name, ".") == 0 || strcmp(namelist[i]->d_name, "..") == 0) 
        {
            free(namelist[i]);
            continue;
        }
        // Print the current entry with tab
        cout << string(space*3, ' ') << namelist[i]->d_name << endl;

        // If the entry is a directory, recurse into it
        if (namelist[i]->d_type == DT_DIR) {
            string new_path = string(path) + "/" + namelist[i]->d_name;
            printDirRec(new_path.c_str(), space + 1);
        }

        free(namelist[i]);
    }

}

void WhoAmICommand::execute() {
    ifstream passwd_file("/etc/passwd");
    string line;
    vector<std::string> chunks;
    uid_t uid = getuid();
    if (uid == static_cast<uid_t>(-1)) {
        perror("smash error: getuid failed");
        return;
    }
    if (passwd_file.is_open()) {
        while (getline(passwd_file, line)) {
            size_t pos = 0;
            string chunk;
            while ((pos = line.find(':')) != string::npos) {
                chunk = line.substr(0, pos);
                chunks.push_back(chunk);
                line.erase(0, pos + 1);
            }
            chunks.push_back(line);

            if (chunks.size() >= 6 && chunks[2] == to_string(uid)) {
                std::cout << chunks[0] << " " << chunks[5] << std::endl;
                passwd_file.close();
                return;
            }
            chunks.clear();
        }
        passwd_file.close();
    }
    else {
        perror("smash error: open failed");
    }
}


void NetInfo::execute() {
    char* args[COMMAND_MAX_ARGS];
    int num_of_args = extractArgs(cmd_line, args);
    if (num_of_args != 2)
    {
        std::cerr << "smash error: netinfo: interface not specified" << std::endl;
    }
    else
    {
        //check if device exists
        string check_interface_cmd = "ip link show " + string(args[1]) + " > /dev/null 2>&1";
        int result = system(check_interface_cmd.c_str());
        if (result != 0) {
            cerr << "smash error: netinfo: interface " << args[1] << " does not exist" << endl;
            return;
        }
        // IP Address and Subnet Mask
        string ip_cmd = "ip -4 addr show " + string(args[1]) + " | grep inet | awk '{print $2}'";
        string ip_cidr = trimNewline(getInfo(ip_cmd.c_str()));
        if (!ip_cidr.empty()) {
            size_t pos = ip_cidr.find('/');
            if (pos != string::npos) {
                string ip = ip_cidr.substr(0, pos);
                int cidr = std::stoi(ip_cidr.substr(pos + 1));
                string subnet = cidr_to_subnet(cidr);
                cout << "IP Address: " << ip << endl;
                cout << "Subnet Mask: " << subnet << endl;
            }
            else {
                cout << "IP Address: " << ip_cidr << endl;
                cout << "Subnet Mask: Unable to determine" << endl;
            }
            //Default Gateway
            string gw_cmd = "ip route | grep default | grep " + string(args[1]) + " | awk '{print $3}'";
            string gateway = trimNewline(getInfo(gw_cmd.c_str()));
            if (!gateway.empty()) {
                cout << "Default Gateway: " << gateway << endl;
            }
            else {
                cout << "Default Gateway: Not configured" << endl;
            }

            // DNS Servers
            string dns_cmd = "grep nameserver /etc/resolv.conf | awk '{print $2}'";
            string dns_servers = trimNewline(getInfo(dns_cmd.c_str()));
            if (!dns_servers.empty()) {
                cout << "DNS Servers: " << dns_servers << endl;
            }
            else {
                cout << "DNS Servers: Not configured" << endl;
            }
        }
        else {
            cerr << "smash error: netinfo: interface " << args[1] << " does not exist" << endl;
        }

    }
}

string NetInfo::getInfo(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        perror("smash error: popen failed");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;

}
string NetInfo::cidr_to_subnet(int cidr) {
    uint32_t mask = 0xFFFFFFFF << (32 - cidr);
    std::stringstream ss;
    ss << ((mask >> 24) & 0xFF) << "."
        << ((mask >> 16) & 0xFF) << "."
        << ((mask >> 8) & 0xFF) << "."
        << (mask & 0xFF);
    return ss.str();
}

string NetInfo::trimNewline(string s) {
    if (!s.empty() && s[s.length() - 1] == '\n') {
        s.erase(s.length() - 1);
    }
    return s;
}

//////////////////////////////---------JobList Implementation---------------////////////////////////////////////////////////

JobsList::JobsList() {
    max_job_id = 0;
}

JobsList::JobEntry::JobEntry(string command, const pid_t pid) {
    if (SmallShell::job_list.jobs.empty()) {
        job_id = 1;
    } else {
        job_id = SmallShell::job_list.max_job_id + 1;
    }

    job_pid = pid;
    isStopped = false;
    cmd_line = std::move(command);
}

void JobsList::addJob(string cmd, pid_t pid) {
    auto* new_job =  new JobEntry(cmd, pid);
    SmallShell::job_list.jobs.push_back(new_job);
    max_job_id += 1;
}


void JobsList::printJobsList() {
    for (auto job: jobs) {
        cout << '[' << job->job_id << "] " << job->cmd_line << endl;
    }
}

void JobsList::killAllJobs() {
    for (auto job: jobs) {
        cout << job->job_pid << ": " << job->cmd_line << endl;
        kill(job->job_pid, SIGKILL);
    }
}

void JobsList::removeFinishedJobs() {
    for (auto it = jobs.begin(); it != jobs.end();) {
        auto job = *it;
        int return_status = waitpid(job->job_pid, nullptr, WNOHANG);
        if (return_status > 0 || return_status == -1) {
            waitpid(job->job_pid, nullptr, 0); // terminate the process, not sure if this is needed
            it = jobs.erase(it);
            delete job;
        } else {
            ++it;
        }
    }
    updateMaxJobId();
}

JobsList::JobEntry *JobsList::getJobById(int jobId) {
    for (auto job: jobs) {
        if (job->job_id == jobId) {
            return job;
        }
    }
    return nullptr;
}

void JobsList::removeJobById(int jobId) {
    for (auto it = jobs.begin(); it != jobs.end();++it) {
        auto job = *it;
        if (job->job_id == jobId) {
            jobs.erase(it);
            delete job;
            return;
        }
    }
    updateMaxJobId();
}

JobsList::JobEntry *JobsList::getLastJob(int *lastJobId) {
    for (auto job: jobs) {
        if (job->job_id == max_job_id) {
            return job;
        }
    }
    *lastJobId = max_job_id;
    return nullptr;
}

JobsList::JobEntry *JobsList::getLastStoppedJob(int *jobId) {
    int max_is_stopped = -1;
    for (auto job: jobs) {
        if (job->isStopped and job->job_id > max_is_stopped) {
            max_is_stopped = job->job_id;
        }
    }
    *jobId = max_is_stopped;
    return getJobById(max_is_stopped);
}

void JobsList::updateMaxJobId() {
    int max_id = -1;
    for (auto job: jobs) {
        if (job->job_id > max_id) {
            max_id = job->job_id;
        }
    }
    max_job_id = max_id;
}