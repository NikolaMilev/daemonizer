#ifndef LOGGER_A_
#define LOGGER_A_

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
//for truncate
#include <unistd.h>
#include <sys/types.h>


#define PATH_LEN_ 20

//Just some delimiters for the timestamp
#define TIMEDELIM_START_ '['
#define TIMEDELIM_END_ ']'
#define TIMEDELIM_DATE_ '-'
#define TIMEDELIM_TIME_ ':'
#define TIMEDELIM_DATE_TIME_ ' '
#define MSGDELIM_START_ '{'
#define MSGDELIM_END_ '}'

//The length of the timestamp
#define TIMESTAMP_LEN_ 24

//This is needed because UNIX like systems return the number of seconds passed since January 1st, 1900. The month ranges
//from 0 to 11, the day ranges from 1 to 31 and the year is the current year subtracted by 1900.
#define ORIGIN_YEAR_ 1900

#define SEPARATE_BY_NEWLINE_ 1

//Main problem remains how to make sure the file is not written to or damaged after recieving a signal from the OS

//I recommend using these
//in combination with changing the calue of LOGPATH_, LOGERRPATH_ or LOGBINPATH_
#define LOG_MSG_(MSG) log_msg_(MSG, LOGPATH_)
#define LOG_ERROR_(ERR) log_msg_(ERR, LOGERRPATH_)
#define LOG_BINARY_(MSG,LEN) log_bin_(MSG, LEN, LOGBINPATH_)
#define UNLOG_BINARY_(ERR_CODE) unlog_bin_full_data_(LOGBINPATH_, ERR_CODE) ;
#define UNLOG_BINARY_PARSED_(ERR_CODE, TIME_, SIZE_) unlog_bin_parsed_(LOGBINPATH_, ERR_CODE, TIME_, SIZE_) ;

//UNIX systems (including Linux distributions and Mac OS) use '\n' as newline character and Windows uses '\n\r'
// " is used here on purpost because you cannot write '\n\r' since that's two characters and not one
#define NEWLINE_STRING_ "\n"

//Size of header (the header will contain a time_t variable, showing the time of the logging and a size_t variable, showing the size of the
//logged data, in that order)
#define HEADER_LEN_ (sizeof(time_t) + sizeof(size_t))

//Variables for the paths for the loggin
char LOGPATH_[PATH_LEN_] ;
char LOGERRPATH_[PATH_LEN_] ;
char LOGBINPATH_[PATH_LEN_] ;
FILE *logFile ;

// These two unions allow me to read sizeof(time_t) (or sizeof(size_t) in the second union) bytes and interpret them as time_t / size_t
// The inability to cast directly is called strict aliasing. For more detail, see: http://stackoverflow.com/questions/98650/what-is-the-strict-aliasing-rule
// A little more explanation:
// Since I cannot treat time_t data as char[sizeof(time_t)], I used the union.
// Unions use as much space as the largest field uses, so, in this case, the same.

typedef union t{
	time_t time_f;
	char char_f[sizeof(time_t)];
}time_str_;

typedef union s{
	time_t size_f;
	char char_f[sizeof(size_t)];
}size_str_;

//Frees the file pointer fp
void free_fp_(FILE **fp) ;

//Does the cleanup: if logFile is opened, logger_cleanup_() function calls free_fp_(&logFile)
//Intended use: signal handling
void logger_cleanup_() ;

// This function's purpose lies solely in creating the timestamp
// Prints a two - digit number to the dest ; If the number has only 1 digit, 0 is appended to the front.
// For numbers > 99 (more than 2 digits), the lowest two digits are observed
void print_small_(char *dest, int val) ;

//Returns a formatted timeprint or NULL, if unsuccessful
//Format: [HH:MM:SS DD-MM-YYY TMZN] where TMZN is not of fixed lentgh and everything else is
char* get_time_() ;

//Prints a message msg to the file stream on path logpath. Opens and closes the path itself.
//Also appends a timestamp obtained using the function get_time_()
int log_msg_(const char* msg, const char* logpath) ;

//Append data buffer to the file with the path logpath. A header is written before the data, uniquely detemrining
//the time of the writing and the size of data
//The first time_t bytes of the record written contain the time of the creation of the record
//and the size_t bytes after that contain the length of the rest of the record
// Therefore, the record written is time_t + size_t + sizeof(unsigned char)*length bytes long
//Notice that sizeof(unsigned char) is 1 but this just seemed a more elegant way to write it.
int log_bin_(const char *data, const size_t length, const char* logpath) ;

//Remove the last record from the binary file and return its contents (as a binary buffer);
//The first time_t bytes of the logpath buffer contain the time of the creation of the record
//and the size_t bytes after that contain the length of the rest of the buffer
// the return buffer is allocated inside the function and, when possible, always fits the size of the header + data perfectly
char* unlog_bin_full_data_(const char* logpath, int *err) ;

// This is a wrapper for unlog_bin_full_data_(), which receives the path, the address of the variable to write error to, the address
// of the time_t variable to write the time part of the header and the size_t variable to write the size part of the header to,
// meaning that the user is not obliged to do the parsing of the data if he/she doesn't want to
char* unlog_bin_parsed_(const char* logpath, int *err, time_t *t, size_t *s) ;

#endif
