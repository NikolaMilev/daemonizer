#include "daemonizer.h"


int do_cleanup_(void)
{
	unlock_(file_fd);
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

int daemonize(void)
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
			return DAEM_FAIL_;
		}
		else
		{
			LOG_ERROR_("Insufficient resources for forking.");
			return DAEM_FAIL_;
		}
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


	//Getting the resource information
	if(getrlimit(RLIMIT_NOFILE, &res_limit) < 0)
	{
		LOG_ERROR_("Cannot close all the files! (reading the resources)");
		return DAEM_FAIL_;
	}

	//If RLIM_INFINITY is reached, we just put 1024 (investigate this in more detail!)
	// RLIM_INFINITY means that there is no limit enforced by the operating system for
	// the number of open file descriptors
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
		return DAEM_FAIL_;
	}

	// Ignoring the SIGHUP signal (meaning a controlling pseudo or virtual terminal has been closed)
	// Thus, this has no use to a daemon and should be ignored
	signal_action.sa_handler = SIG_IGN;
	sigemptyset(&signal_action.sa_mask);
	signal_action.sa_flags = 0;
	if (sigaction(SIGHUP, &signal_action, NULL) < 0)
	{
		LOG_ERROR_("Can't ignore SIGHUP");
		return DAEM_FAIL_;
	}

	// The second fork and the ignoring of SIGHUP signal make sure that this process
	// can never get a controlling tty (setsid makes a proces the leader of the session and fork makes a child,
	// which is not the leader and its parent has exited)
	pid = fork();
	if(pid < 0)
	{
		LOG_ERROR_("Second fork failed!");
		return DAEM_FAIL_;
	}
	else if(pid > 0)
	{
		exit(EXIT_SUCCESS);
	}

	// When the program receives SIGTERM signal, it should gracefully terminate,
	// thus we do a cleanup in the signal_handler
	if(signal(SIGTERM, signal_handler) == SIG_ERR)
	{
		//This should actually never happen
		LOG_ERROR_("Bad signal number!");
		return DAEM_FAIL_;
	}

	//Changing the dir to a safe place (for logging, loading files, etc)
	if(chdir(WORKDIR_) < 0)
	{
		LOG_ERROR_("Cannot change directory!");
		return DAEM_FAIL_;
	}

	if(is_running_file_() != RUN_NOT_RUNNING_)
	{
		LOG_ERROR_("Another process instace already running!");
		exit(EXIT_FAILURE);
	}

	return DAEM_SUCC_;
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

int is_running_socket_()
{

 	struct sockaddr_un socket_var;

	if((socket_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	{
		return RUN_FAIL_ ;
	}

	bzero(&socket_var, sizeof(socket_var));
	socket_var.sun_family = AF_UNIX;
	snprintf(socket_var.sun_path, MAX_SOCK_PATH_LEN_, "%s", LOCK_PATH_);

	if(bind(socket_fd, (struct sockaddr*) &socket_var, sizeof(socket_var)) < 0)
	{
		if(errno == EADDRINUSE)
		{
			return RUN_RUNNING_;
		}
		else
		{
			return RUN_FAIL_;
		}
	}
	return RUN_NOT_RUNNING_ ;
}

int is_running_file_()
{
	file_fd =  open(LOCK_PATH_, O_CREAT | O_RDWR,  S_IRWXU);

	if(lock_(file_fd) < 0)
	{
		if(errno == EAGAIN || errno == EACCES)
		{
			return RUN_RUNNING_ ;
		}
		return RUN_FAIL_;
	}
	return RUN_NOT_RUNNING_;
}

int lock_(int fd)
{
    struct flock lock;

    lock.l_type = F_WRLCK ;     	/* F_RDLCK, F_WRLCK, F_UNLCK */
    lock.l_start = 0;  				/* byte offset, relative to l_whence */
    lock.l_whence = SEEK_SET; 	/* SEEK_SET, SEEK_CUR, SEEK_END */
    lock.l_len = 0;      		 	/* #bytes (0 means to EOF) */

    return fcntl(fd, F_SETLK, &lock);
}

int unlock_(int fd)
{
	struct flock lock;

	lock.l_type = F_UNLCK;     	/* F_RDLCK, F_WRLCK, F_UNLCK */
	lock.l_start = 0;  				/* byte offset, relative to l_whence */
	lock.l_whence = SEEK_SET; 		/* SEEK_SET, SEEK_CUR, SEEK_END */
	lock.l_len = 0; 					/* #bytes (0 means to EOF) */

	return(fcntl(fd, F_SETLK, &lock));
}
