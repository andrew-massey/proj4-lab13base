#ifndef LAB13_H
#define LAB13_H

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <unistd.h>
#include <signal.h>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <array>
#include <termios.h>

using std::vector;
using std::array;

int lastExitStatus;
std::string jobArg;

int shl_processes;
pid_t shellPG;
int shell_terminal;
bool foregroundJob;
vector<vector<std::string>> shl_argv;
//vector<vector<std::string>> jobsList;

std::string shl_stdin;
std::string shl_stdout;
std::string shl_stderr;

int shellLoop();
void outputPrinter();
void cmdParser(std::string cmd);

//Command running/job control stuff be below here
vector<char *> convertVector(vector<std::string> &stringVector);
void deleteVector(vector<char *> & charVector);
void execProg(vector<std::string> stringArgs);
void close_pipe(int pipefd[2]);
void commandRunner();
int exitStatusHandler(int pid, int status);
void printJobs();
void jobToForeground(pid_t pgid, bool cont);
void jobToBackground(pid_t pgid, bool cont);
void jobWaiter(pid_t pgid);
void continueJob(pid_t pgid, bool foreground);
int findJob(pid_t pgid);

int isJobComplete(struct job jobCheck);
int isJobStopped(struct job jobCheck);
int setProcessStatus(pid_t pid, int status);
void updateStatus();
void jobNotification();

struct process
{
  pid_t pid;
  bool completed = false;
  bool stopped = false;
  int status;
};

struct job
{
  std::string command;
  std::string status;
  vector<process> processes;
  pid_t pgid;
  bool notified = false;
};

vector<job> vectorJobs;

#endif
