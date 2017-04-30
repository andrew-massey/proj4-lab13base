#ifndef JOBCONTROL_H
#define JOBCONTROL_H

#include "Lab13.h"

using std::vector;
using std::string;

extern vector<vector<std::string>> jobsList;

int exitStatusHandler(int pid, int status);
void printJobs();
void jobToForeground(pid_t pgid, bool cont);
void jobToBackground(pid_t pgid, bool cont);
void jobWaiter(pid_t pgid);
void continueJob(pid_t pgid, bool foreground);
int findJob(pid_t pgid);
vector<char *> convertVector(vector<std::string> &stringVector);
void deleteVector(vector<char *> & charVector);
void execProg(vector<std::string> stringArgs);
void close_pipe(int pipefd[2]);
void commandRunner();

#endif
