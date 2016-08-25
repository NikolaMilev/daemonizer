#include "logger.h"

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

//The mask of 022 allows that any created files are writable and readable by the owner
//and only readable by the rest
#define UMASK_ 022

#define WORKDIR_ "/home/reaponja/Desktop/daemon/log"

int do_cleanup_(void)
{
	return 1;
}

static void signal_handler(int signo)
{
	if(signo == SIGTERM)
	{
		int retval = do_cleanup_();
		if(retval > 0)
		{
			LOG_MSG_("Closing the server with successful cleanup!");
		}
		else
		{
			LOG_MSG_("Closing the server with unsuccessful cleanup!");
		}
	}
	exit(EXIT_SUCCESS);
}

void daemonize(void)
{
	pid_t pid;
	int fd0, fd1, fd2, ctr;
	struct rlimit res_limit;
	struct sigaction signal_action;

	umask(UMASK_);
	pid = fork();

	//Something gone wrong
	if(pid < 0)
	{
		//ENOSYS means that fork is not supported on this platform
		if(errno == ENOSYS)
		{
			LOG_ERROR_("Forking not supported on this platform.");
		}
		else
		{
			LOG_ERROR_("Insufficient resources for forking.");
		}
		exit(EXIT_FAILURE);
	}
	//We are in the parent, exiting
	else if(pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	//Else, we are in the child process
	////////////////////////////////////////////////////////////////////////////

	//Creating a new session where this is the only process
	setsid();

	//Changing the dir to a safe place (for logging, loading files, etc)
	if(chdir(WORKDIR_) < 0)
	{
		LOG_ERROR_("Cannot change directory!");
	}

	//Getting the resource information
	if(getrlimit(RLIMIT_NOFILE, &res_limit) < 0)
	{
		LOG_ERROR_("Cannot close all the files! (reading the resources)");
		exit(EXIT_FAILURE);
	}

	//If RLIM_INFINITY is reached, we just put 1024 (investigate this in more detail!)
	if (res_limit.rlim_max == RLIM_INFINITY)
	{
		res_limit.rlim_max = 1024;
	}

	//Closing all the file descriptors
   for (ctr = 0; ctr < res_limit.rlim_max; ctr++)
   {
		close(ctr);
	}

	//Opening file descriptors 0, 1 and 2 (which represent stdin, stdout and stderr, respectively) to point to "/dev/null"
	fd0 = open("/dev/null", O_RDWR);
	fd1 = dup(0);
	fd2 = dup(0);
	//Checking for errors in the operations above
	if(fd0 != 0 || fd1 != 1 || fd2 != 2)
	{
		LOG_ERROR_("Cannot replace 0, 1 and 2 file descriptors!");
		exit(EXIT_FAILURE);
	}

	// Ignoring the SIGHUP signal (meaning a controlling pseudo or virtual terminal has been closed)
	// Thus, this has no use to a daemon and should be ignored
	signal_action.sa_handler = SIG_IGN;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_flags = 0;
	if (sigaction(SIGHUP, &signal_action, NULL) < 0)
	{
		LOG_ERROR_("Can't ignore SIGHUP");
		exit(EXIT_FAILURE);
	}

	// The second fork and the ignoring of SIGHUP signal make sure that this process
	// can never get a controlling tty (do some more research)_
	pid = fork();
	if(pid < 0)
	{
		LOG_ERROR_("Second fork failed!");
		exit(EXIT_FAILURE);
	}
	else if(pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	// When the program receives SIGTERM signal, it should gracefully terminate,
	// thus we do a cleanup in the signal_handler
	if(signal(SIGTERM, signal_handler) == SIG_ERR)
	{
		LOG_ERROR_("Bad signal number!");
	}

	//STILL LEFT TO DO SIGNAL HANDLING, SINGLE INSTANCE, etc. See notebook
}

int main(void)
{
	daemonize();

	while(1)
	{
		sleep(5);
		LOG_MSG_("Hello, bitches!");
	}
	return 0;
}
