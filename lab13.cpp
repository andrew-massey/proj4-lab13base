#include "Lab13.h"

/*
 * Main program entry.
 */
int main()
{
  setvbuf(stdout, NULL, _IONBF, BUFSIZ); //Set output to unbuffered
  int exitChecker;

  //Enter the shell loop
  if ((exitChecker = shellLoop()) == -1)
      printf("Unexpected termination.\n");
  else
    printf("Exiting 1730sh.\n");

  return exitChecker;

} //main

/*
 * The shell loop! Does nothing but loop until a proper exit.
 */
int shellLoop()
{
  shl_processes = 0;
  shl_stdin = "STDIN_FILENO";
  shl_stdout = "STDOUT_FILENO";
  shl_stderr = "STDERR_FILENO";
  int shellStatus = 1;

  //JOB CONTROL BLOCK
  shell_terminal = STDIN_FILENO; //JOB CONTROL - SETTING THE TERMINAL!
  lastExitStatus = 0; //JOB CONTROL - STATUS
  foregroundJob = true; //JOB CONTROL - STUFF
  //Ignore a bunch of signals
  signal(SIGINT, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  //signal(SIGCHLD, SIG_IGN);
  //Put shell in its own process group
  shellPG = getpid();
  if (setpgid (shellPG, shellPG) < 0)
    {
      perror ("Shell process group");
      exit(EXIT_FAILURE);
    }
  tcsetpgrp(shell_terminal, shellPG);
  //END OF JOB CONTROL STUFF  


  //Loop until bad status
  while (shellStatus >= 0)
    {
      std::string cmd;                //cmd input
   
      jobNotification();

      //Get current dir and fiddle with it for pretty ~ prompts
      char * rawDirName = get_current_dir_name();
      std::string homePath = getenv("HOME");
      std::string currentDirName(rawDirName);
      if (!strncmp(currentDirName.c_str(), homePath.c_str(), homePath.size()))
	{ //We're in a sub directory of home, ~ it up
	  std::string truncDirName = currentDirName.substr(homePath.size(),std::string::npos);
	  printf("1730sh:~%s$ ", truncDirName.c_str());
	}
      else //We're not in a subdirectory of home, don't ~
	printf("1730sh:%s$ ", currentDirName.c_str()); //shell prompt
      free(rawDirName);

      std::getline(std::cin, cmd);    //Get input
      if (std::cin.eof() == 1) //Break if the user ctrl+Ds
	cmd = "exit";

      jobArg = cmd;

      if (cmd.length() > 0) //Only if an actual command input
	{
	  cmdParser(cmd); //Pass to the parsing function

	  if (shl_argv.size() > 0) //If the argument list is valid
	    {
	      if (shl_argv[0].at(0) == "exit") //"Exit" input entered
		{
		  if (shl_argv[0].size() == 1) //No custom exit status
		    return lastExitStatus;
		  else //Custom exit status, use it
		    {//But make sure it's valid first
		      char * eCheck = (char *)shl_argv[0].at(1).c_str();
		      long converter = strtol(shl_argv[0].at(1).c_str(), &eCheck, 10);
		       
		      if (*eCheck) //Not a valid number!
			return lastExitStatus;
		      else //Valid number
			return converter;
		    }
		}
	      else if (shl_argv[0].at(0) == "cd") //"cd" input entered
		{ //No argument = go home
		  if (shl_argv[0].size() == 1)
		    chdir(homePath.c_str());
		  else
		    { //Argument = go there
		      if (chdir(shl_argv[0].at(1).c_str()) == -1)
			perror("-1730sh: cd");
		    }
		}
	      else if (shl_argv[0].at(0) == "jobs") //JOB CONTROL - JOB LIST
		{
		  printJobs();
		}
	      else if (shl_argv[0].at(0) == "fg") //JOB CONTROL - FOREGROUND
		{
		  pid_t fgpid = atoi(shl_argv[0].at(1).c_str());
		  continueJob(fgpid, true);
		}
              else if (shl_argv[0].at(0) == "bg") //JOB CONTROL - BACKGROUND
                {
                  pid_t bgpid = atoi(shl_argv[0].at(1).c_str());
                  continueJob(bgpid, false);
                }

	      else if (shl_argv[0].at(0) == "help") //"Help" input entered"
		{
		  if (shl_argv[0].size() == 1) //No argument, show all topics
		    printf("Available help topics:\n%s\n%s\n%s\n", "cd", "help [topic]", "exit");
		  else if (shl_argv[0].at(1) != "help" && shl_argv[0].at(1) != "cd" && shl_argv[0].at(1) != "exit") //Invalid help topic
		    printf("Invalid help topic.\n");
		  else if (shl_argv[0].at(1) == "help") //Help help
		    printf("Get help for various commands supported by the shell.\n%s\n%s\n", "\nFormat: help [topic]\n", "Example: help help");
		  else if (shl_argv[0].at(1) == "cd") //Help cd
		    printf("Change the current working directory.\n%s\n%s\n","\nFormat: cd [PATH]\n", "Example: cd myDirectoryName");
		  else if (shl_argv[0].at(1) == "exit") //Help exit
		    printf("Exit the shell. You can exit with the status of the last exited job or a custom status.\n%s\n%s\n",
			   "\nFormat: exit [OPTIONAL STATUS INTEGER]\n", "Example: exit 1");
		}
	      else //Not a builtin, must be a command
		  commandRunner();
	    } //Exiting arg size if
	} //Exiting cmd size if

      /*      
      //If everything's cool, print the job parse output
      if (shellStatus >= 0)          
	outputPrinter();
      */
      //RESET EVERYTHING BACK TO DEFAULT
      shl_stdin = "STDIN_FILENO";
      shl_stdout = "STDOUT_FILENO";
      shl_stderr = "STDERR_FILENO";
      shl_processes = 0;
      foregroundJob = true; //RESET JOB CONTROL VARIABLE
      shl_argv.clear();
      jobArg.clear(); //RESET JOBARG
    }

  return 0;
}

/*
 * Parses the command input. Very ugly.
 * @param cmd The string to be parsed character by character.
 */
void cmdParser(std::string cmd)
{
  enum parseTypes {
    appendOut, truncateOut, redirectIn, errorAppend, errorTruncate, regular
  };
  parseTypes wordTypes = regular;
  size_t pos = 0;
  size_t startSub = 0;
  bool inQuotes = false;
  bool wordFinished = false;
  bool reset = false; //Needed to break out of the loop for certain characters

  //The array to temporarily store things
  vector<std::string> cmdArgs;

  if (cmd[cmd.length() - 1] == '&')
    {
      foregroundJob = false;
      cmd.erase((cmd.length() - 1),1); 
    }

  //The big loop: continue until the string is finished
  while (pos < cmd.length())
    {
      if (!wordFinished) //If the word isn't finished, go here
      	{
	  if (reset) //If we have reset to be here, clear the reset flag
	    reset = false;

	  //Not a quote, fast forward through spaces
	  while (!inQuotes && isspace(cmd[pos]) && pos < cmd.length())
	    {
	      pos++;
	      startSub++;
	    }

	  //We're entering a quote, enable special handling
	  if (cmd[pos] == '"' && cmd[pos-1] != '\\')
	    {
	      inQuotes = true;
	      pos++;
	      startSub++;
	    }

	  //Loop through non-space characters not in quotes
	  while (!inQuotes && !wordFinished && !isspace(cmd[pos]) && pos < cmd.length())
	    {
	      //If the start symbol is a special character, prepare to
	      //handle it in a special way
	      if (startSub == pos && ((cmd[pos] == '>') || (cmd[pos] == '<') || (cmd[pos] == 'e' && cmd[pos+1] == '>') ||cmd[pos] == '|'))
	      	{
		  if (cmd[pos] == '>') //Output handling
		    {
		      pos++;
		      startSub++;	      
		      if (cmd[pos] == '>') //>> = append
			{
			  pos++;
			  startSub++;
			  wordTypes = appendOut;
			}
		      else // > = truncate
			wordTypes = truncateOut;
		    }
		  
		  else if (cmd[pos] == '<')
                    { // < = input redirect
		      wordTypes = redirectIn;
		      pos++;
		      startSub++;
                    }
		  
                  else if (cmd[pos] == 'e')
                    { // e> = truncate
		      pos+=2;
                      startSub+=2;
                      if (cmd[pos] == '>')
                        {// e>> = append
                          pos++;
                          startSub++;
			  wordTypes = errorAppend;
			}
		      else
			wordTypes = errorTruncate;
		    }		  
		  
		  else if (cmd[pos] == '|')
                    { //Pipe = new process
		      if (cmdArgs.size() > 0) //Don't break on empty vectors
			{
			  shl_argv.push_back(cmdArgs);
			  cmdArgs.clear();
			}
		      pos++;
		      startSub++;
		      wordTypes = regular;
		    }
		  
		  reset = true; //Break back to the beginning loop
		  
	      	} //EXITING STARTPOS == WEIRD SYMBOL IF STATEMENT
	      else //IF ANYTHING ELSE - regular characters
	      	{
		  if (cmd[pos] == '"' && cmd[pos-1] != '\\')
		    { //This is a quote in a normal word, so we get rid of it
		      cmd.erase(pos,1);
		      pos--;
		      continue;
		    }
		  
		  //This is a special character in the middle of a string, finish the word
		  if ((cmd[pos] == '>') || (cmd[pos] == '<') || (cmd[pos] == 'e' && cmd[pos+1] == '>') || (cmd[pos] == '|'))
		      wordFinished = true;
		  
		  //If the word's not finished yet, move forward
		  if (!wordFinished)
		    pos++;
		  
		}
	      
	      if (reset) //Reset = break out of this loop too
		break;

	    }//EXITING THE NON-QUOTE WHILE LOOP - HIT A SPACE OR END	  
	 
	  if (reset) //Reset = restart the main loop
	    continue;
	  
	  //We're in a quotation, fast forward through the entire string
	  while (inQuotes && !wordFinished && pos < cmd.length())
	    {
	      if (cmd[pos] == '"' && cmd[pos-1] != '\\')
		{ //This is the end of the quote
		  cmd.erase(pos,1); //Get rid of the quotation mark
		  pos--;
		  wordFinished = true;
		  inQuotes = false;
		}
	      
	      pos++;
	    }

	}//Exiting "if !WordFinished"	  
      
      wordFinished = true; //I mean we have to be done if we're here, right?

      if (wordFinished) //Handle finished words
	{
	  std::string temp(cmd.substr(startSub, pos-startSub));
	  
	  //Removing the \ from escape sequence quotes
	   for (uint i = 0; i < temp.length(); i++)
	    {
	      if (temp[i] == '\\' && temp[i+1] == '"')
		temp.erase(i,1);
	    }

	   //If the string isn't empty, handle it! 
	   if (temp.length() > 0)
	     {
	       switch (wordTypes)
		 {
		 case appendOut:
		   {//Set append out
		     shl_stdout = temp;
		     shl_stdout += " (append)";
		     break;
		   }
		 case truncateOut:
		   {//Set truncate out
		     shl_stdout = temp;
		     shl_stdout += " (truncate)";
		     break;
		   }
		 case redirectIn:
		   {//Set input redirection
		     shl_stdin = temp;
		     break;
		   }
		 case errorTruncate:
		   {//Set error truncate redirection
		     shl_stderr = temp;
		     shl_stderr += " (truncate)";
		     break;
		   }
		 case errorAppend:
		   {//Set error append redirection
		     shl_stderr = temp;
		     shl_stderr += " (append)";
		     break;
		   }
		 default:
		   {//Normal word, put it in the array
		     cmdArgs.push_back(temp);
		     break;
		   }
		 }
	       wordTypes = regular;
	     }
	   
	   //Reset our variables
	   startSub = pos;
	   inQuotes = wordFinished = false;
	}  
    }//Exiting entire while loop

  //Push whatever is left to the thing
  if (cmdArgs.size() > 0)
      shl_argv.push_back(cmdArgs);

  shl_processes = shl_argv.size();
  cmdArgs.clear();
}

/*
 * Prints our job info and process stuff.
 */
void outputPrinter()
{
  printf("\n");
  printf("Job STDIN = %s\n", shl_stdin.c_str());
  printf("Job STDOUT = %s\n", shl_stdout.c_str());
  printf("Job STDERR = %s\n", shl_stderr.c_str());

  printf("\n");

  printf("%d pipe(s)\n", shl_processes > 0 ? shl_processes - 1 : 0);
  printf("%d process(es)\n", shl_processes);

  printf("\n");

  for (unsigned int i = 0; i < shl_argv.size(); i++)
    {
      printf("Process %d argv:\n", i);
      for (unsigned int j = 0; j < shl_argv[i].size(); j++)
	{
	  printf("%d: %s\n", j, shl_argv[i].at(j).c_str());
	}
      printf("\n");
    }
}


//JOB CONTROL STUFF IS EVERYTHING BELOW HERE
/*
 * Runs our programs
 */
void commandRunner()
{
  int pid;
  pid_t pgid;
  struct job newJob;

  vector<array<int, 2>> pipes; //Our pipe arrays

  newJob.command = jobArg;
  newJob.status = "Running";

  //CYCLE THROUGH ARGUMENT ARRAYS
  for (unsigned int i = 0; i < shl_argv.size(); ++i)
    {    
      struct process newProcess;

      if (i != shl_argv.size() - 1) //Not the last command
	{
	  int pipefd [2]; //Make some pipes
	  if (pipe(pipefd) == -1)
	    {
	      perror("-1730sh: pipe");
	      exit(EXIT_FAILURE);
	    } //if
	  pipes.push_back({pipefd[0], pipefd[1]});
	} // if
      
      //Start FORKING processes
      if ((pid = fork()) == -1)
	{
	  perror("-1730sh: fork");
	  exit(EXIT_FAILURE);
	}
      else if (pid == 0) //In the child process
	{
	  //Reset the ignores to default for child
	  signal(SIGINT, SIG_DFL);
	  signal(SIGQUIT, SIG_DFL);
	  signal(SIGTSTP, SIG_DFL);
	  signal(SIGTTIN, SIG_DFL);
	  signal(SIGTTOU, SIG_DFL);
	  signal(SIGCHLD, SIG_DFL);

	  pid = getpid();
	  if (!pgid)
	    pgid = pid;
	  setpgid(pid, pgid);
	  
	  if (i != 0)
	    { //Not the first command
	      if (dup2(pipes.at(i-1)[0], STDIN_FILENO) == -1)
		{
		  perror("-1730sh: dup2");
		  exit(EXIT_FAILURE);
		}
	    }
	  if (i != shl_argv.size() - 1)
	    { //Not the last command
	      if (dup2(pipes.at(i)[1], STDOUT_FILENO) == -1)
		{
		  perror("-1730sh: dup2");
		  exit(EXIT_FAILURE);
		}
	    }

	  //close all pipes created so far
	  for (unsigned int i = 0; i < pipes.size(); ++i)
	    close_pipe(pipes.at(i).data());
	  //Finally, let's make a program!
	  execProg(shl_argv.at(i));
	
	} // end if-child
      else
	{
	  newProcess.pid = pid;
	  if (!pgid)
	    {
	      newJob.pgid = pid;
	      pgid = pid;
	    }
	  setpgid(pid, pgid);
	}
      newJob.processes.push_back(newProcess);
    } // end for all commands

  vectorJobs.push_back(newJob);

  // close all pipes
  for (unsigned int i = 0; i < pipes.size(); ++i)
    close_pipe(pipes.at(i).data());
  
  if (foregroundJob)
    jobToForeground(pgid, false);
  else
    jobToBackground(pgid, false);
  
}

/*
 * Converts a vector of strings into a vector of characters. Adds nullptr
 * as the last element of the vector.
 * @param stringVector The vector of strings to be converted.
 * @return The new vector of characters.
 */
vector<char *> convertVector(vector<std::string> &stringVector)
{
  vector<char *> charVector;

  for (unsigned int i = 0; i < stringVector.size(); ++i)
    {
      charVector.push_back(new char [stringVector.at(i).size() + 1]);
      strcpy(charVector.at(i), stringVector.at(i).c_str());
    }
  charVector.push_back(nullptr);
  
  return charVector;
  }

/*
 * Deletes our dynamically allocated character array from conversion.
 * @param charVector Vector to be deleted.
 */
void deleteVector(vector<char *> & charVector)
{
  for (unsigned int i = 0; i < charVector.size(); ++i)
    delete[] charVector.at(i);
}

/*
 * Executes the program with an argument list of strings. Sends the
 * strings to be converted to characters as appropriate first.
 * @param stringArgs Vector of strings to be passed to program.
 */
void execProg(vector<std::string> stringArgs)
{
  vector<char *> charArgs = convertVector(stringArgs);
  execvp(charArgs.at(0), &charArgs.at(0));
  perror("-1730sh: "); //This only occurs if we done goofed
  deleteVector(charArgs);
  exit(EXIT_FAILURE);
}

/*
 * Closes the pipes!
 * @param pipefed The array of pipes to be closed
 */
void close_pipe(int pipefd [2])
{
  if (close(pipefd[0]) == -1)
    {
      perror("-1730sh: ");
      exit(EXIT_FAILURE);
    }
  if (close(pipefd[1]) == -1)
    {
      perror("-1730sh: ");
      exit(EXIT_FAILURE);
    }
    }

/*
 * Handles exit status printouts after we've waited.
 * @param pid The PID of the process that was signaled
 * @param status The status we're checking
 * @return The exit status we found
 *//*
int exitStatusHandler(int pid, int status)
{
  int job = findJob(pid);

  if (WIFEXITED(status)) //Normal exit
    {
      lastExitStatus = WEXITSTATUS(status);
      if (job != -1)
	jobsList.erase(jobsList.begin() + job);
      printf("%d\tExited (%d)\t%s\n", pid, lastExitStatus, jobArg.c_str());
    }
  else if (WIFSTOPPED(status)) //Stopped
    {
      if (job != -1)
	jobsList[job].at(1) = "Stopped";
      printf("%d\tStopped\t%s\n", pid, jobArg.c_str());
    }
  else if (WIFCONTINUED(status)) //Continued a stop
    {
      if (job != -1)
	jobsList[job].at(1) = "Running";
      printf("%d\tContinued\t%s\n", pid, jobArg.c_str());
    }
  else if (WIFSIGNALED(status)) //Abnormal exit
    { 
      if (job != -1)
	jobsList.erase(jobsList.begin() + job);
      lastExitStatus = WTERMSIG(status);
      printf("%d\tAbnormal termination (%d)\t%s\n", pid, lastExitStatus, jobArg.c_str());
    }
  
    return lastExitStatus;
    }*/

//Print the list of all active jobs
void printJobs()
{
  if (vectorJobs.size() > 0)
    printf("JID\tStatus\t\tCommand\n");

  for (unsigned int i = 0; i < vectorJobs.size(); i++)
    {
      printf("%d\t%s\t\t%s\n", vectorJobs[i].pgid, vectorJobs[i].status.c_str(), vectorJobs[i].command.c_str());
    }

}

//Sends the job to the foreground
void jobToForeground(pid_t pgid, bool cont)
{
  tcsetpgrp(shell_terminal, pgid);

  // Send the job a continue signal if needed
  if (cont)
    {
      if (kill (-pgid, SIGCONT) < 0)
        perror ("kill (SIGCONT)");
    }
  
  jobWaiter(pgid);

  //Put shell back into foreground
  tcsetpgrp(shell_terminal, shellPG);
}

//Sends the job to the background
void jobToBackground(pid_t pgid, bool cont)
{
  // Send the job a continue signal if called
  if (cont)
    {
      if (kill (-pgid, SIGCONT) < 0)
	perror ("kill (SIGCONT)");
    }
}

//Waits on the job
void jobWaiter(pid_t pgid)
{
  pid_t wpid;
  int status;

  do
    {
      wpid = waitpid(WAIT_ANY, &status, WUNTRACED);
    } while (!setProcessStatus(wpid, status) && !isJobComplete(vectorJobs[findJob(pgid)]) && !isJobStopped(vectorJobs[findJob(pgid)]));
  
  //  tcsetpgrp(shell_terminal, shellPG);
   

  //exitStatusHandler(pgid, status);
}

//Continues the job
void continueJob(pid_t pgid, bool foreground)
{
  int job = findJob(pgid);
  
  if (job != -1)
      vectorJobs[job].status = "Running";
  
  if (foreground)
    jobToForeground (pgid, true);
  else
    jobToBackground (pgid, true);
}

//Finds the vector with the job in the jobsList
int findJob(pid_t pgid)
{
  for (unsigned int i = 0; i < vectorJobs.size(); i++)
    {
      if (vectorJobs[i].pgid == pgid)
	return i;
    }
  return -1;
}

int isJobComplete(struct job jobCheck)
{
  for (unsigned int i = 0; i < jobCheck.processes.size(); i++)
    {
      if (!jobCheck.processes[i].completed)
	return 0;
    }
  return 1;
}

int isJobStopped(struct job jobCheck)
{
  for (unsigned int i = 0; i < jobCheck.processes.size(); i++)
    {
      if (jobCheck.processes[i].stopped)
        return 1;
    }

  return 0;

}

int setProcessStatus(pid_t pid, int status)
{
  if (pid > 0)
    {
      for (unsigned int i = 0; i < vectorJobs.size(); i++)
        for (unsigned int j = 0; j < vectorJobs[i].processes.size(); j++)
          if (vectorJobs[i].processes[j].pid == pid)
            {
              vectorJobs[i].processes[j].status = status;
              if (WIFSTOPPED (status))
                vectorJobs[i].processes[j].stopped = true;
              else
                {
                  vectorJobs[i].processes[j].completed = true;
                  if (WIFSIGNALED (status))
                    printf("%d terminated by signal (%d)\n", pid,
			   WTERMSIG (vectorJobs[i].processes[j].status));
                }
              return 0;
	    }
      printf("No child process %d\n", pid);
      return -1;
    }
  else if (pid == 0 || errno == ECHILD)
    return -1; //No processes are done
  else
    { //Other stuff is broken
      perror ("waitpid");
      return -1;
    }
}

void updateStatus()
{
  int status;
  pid_t pid;
  
  do
    pid = waitpid (WAIT_ANY, &status, WUNTRACED|WNOHANG);
  while (!setProcessStatus (pid, status));
}


void jobNotification()
{
  updateStatus(); //Update child process stuff

  for (unsigned int i = 0; i < vectorJobs.size(); i++)
    {
      if (isJobComplete(vectorJobs[i])) //If everything's done, tell the user and delete it
	{
	  printf("%d\t%s\t\t%s\n", vectorJobs[i].pgid, "Exited", vectorJobs[i].command.c_str());
	  vectorJobs.erase(vectorJobs.begin() + i);
	  i--;
	}
      else if (isJobStopped(vectorJobs[i]) && !vectorJobs[i].notified)
	{ //If things have stopped, tell the user
	  printf("%d\t%s\t\t%s\n", vectorJobs[i].pgid, "Stopped", vectorJobs[i].command.c_str());
	  vectorJobs[i].status = "Stopped";
	  vectorJobs[i].notified = true;
	}
    }
}
