#ifndef DAEMONIZER_
#define DAEMONIZER_

#include "logger.h"

#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/un.h>
#include <sys/file.h>

#define DAEM_FAIL_ -1
#define DAEM_SUCC_ 0




#define write_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_WRLCK, (offset), (whence), (len))
#define un_lock(fd, offset, whence, len) lock_reg((fd), F_SETLK, F_UNLCK, (offset), (whence), (len))

#define RUN_FAIL_ -1
#define RUN_RUNNING_ 1
#define RUN_NOT_RUNNING_ 0

//The mask of 022 allows that any created files are writable and readable by the owner
//and only readable by the rest
#define UMASK_ 022

int file_fd;

#define WORKDIR_ "/home/reaponja/Desktop/daemon/daemonizer"
#define LOCK_PATH_ "/home/reaponja/Desktop/daemon/lock_file"
#define MAX_SOCK_PATH_LEN_ 60

int do_cleanup_(void) ;
static void signal_handler(int signo) ;
int daemonize(void) ;
int is_running_file_() ;

pid_t lock_test(int fd) ;
int lock_(int fd) ;
int unlock_(int fd) ;

#endif
