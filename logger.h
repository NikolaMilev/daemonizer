#ifndef LOGGER_A_
#define LOGGER_A_

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
//for truncate
#include <unistd.h>
#include <sys/types.h>

//Errno is not altered in any of the functions and can be used after they fail

#define PATH_LEN_ 20

//Just some delimiters for the timestamp
#define TIMEDELIM_START_ '['
#define TIMEDELIM_END_ ']'
#define TIMEDELIM_DATE_ '-'
#define TIMEDELIM_TIME_ ':'
#define TIMEDELIM_DATE_TIME_ ' '

//Actually not used, can be introduced if need be
#define MSGDELIM_START_ '{'
#define MSGDELIM_END_ '}'

// Return values for log_msg_() and log_bin_() and error values for unlog_bin_full_data_() and unlog_bin_parsed_()
#define OK_ 0
#define MALLOC_FAIL_ -1
#define OPEN_FAIL_ -2
#define READ_FAIL_ -3
#define BAD_LOG_PATH_ -4
#define WRITE_FAIL_ -5

//The length of the timestamp
#define TIMESTAMP_LEN_ 24

//This is needed because UNIX like systems return the number of seconds passed since January 1st, 1900. The month ranges
//from 0 to 11, the day ranges from 1 to 31 and the year is the current year subtracted by 1900.
#define ORIGIN_YEAR_ 1900

//Main problem remains how to make sure the file is not written to or damaged after recieving a signal from the OS

//I recommend using these
//in combination with changing the calue of LOGPATH_, LOGERRPATH_ or LOGBINPATH_
#define LOG_MSG_(MSG, TIMESTAMP_) log_msg_(MSG, TIMESTAMP_, LOGPATH_)
#define LOG_MSG_NOW_(MSG) log_msg_now_(MSG, LOGPATH_)
#define LOG_ERROR_(ERR, TIMESTAMP_) log_msg_(ERR, TIMESTAMP_, LOGERRPATH_)
#define LOG_ERROR_NOW_(ERR) log_msg_now_(ERR, LOGERRPATH_)
#define LOG_BINARY_(MSG,LEN) log_bin_(MSG, LEN, LOGBINPATH_)
#define READ_BINARY_(ERR_CODE) read_bin_full_data_(LOGBINPATH_, ERR_CODE) ;
#define READ_BINARY_PARSED_(ERR_CODE, TIME_, SIZE_) read_bin_parsed_(LOGBINPATH_, ERR_CODE, TIME_, SIZE_) ;
#define TRUNC_BIN_(AMOUNT) truncate_by_(LOGBINPATH_, AMOUNT)
#define TRUNC_LAST_BIN_() truncate_last_bin_(LOGBINPATH_)

//UNIX systems (including Linux distributions and Mac OS) use '\n' as newline character and Windows uses '\n\r'
// string (char*) is used here on purpose because you cannot write '\n\r' into one char variable since that's two characters and not one
char NEWLINE_STRING_[3] ;
// Indicates if a newline string is to be used while doing non-binary logging.
char SEPARATE_BY_NEWLINE_ ;

//Size of header (the header will contain a time_t variable, showing the time of the logging and a size_t variable, showing the size of the
//logged data, in that order)
#define HEADER_LEN_ (sizeof(time_t) + sizeof(size_t))

//Variables for the paths for the loggin
char LOGPATH_[PATH_LEN_] ;			// Log file path
char LOGERRPATH_[PATH_LEN_] ;		// Error log file path
char LOGBINPATH_[PATH_LEN_] ;		// Binary log file path
FILE *logFile ;						// Universal FILE* variable used for all the logging attempts

// The size of the last read message, with the header. It is reset to 0 every time we call truncate_by_()
size_t LAST_READ_SIZE_ ;

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

//Does the cleanup: if logFile is opened, logger_cleanup_() function closes the file and sets the pointer to NULL
//Intended use: signal handling
void logger_cleanup_() ;

// Wrapper around fopen() call that retries if the call was interrupted by a signal
// Arguments: the same as fopen()
// Returns: the same as fopen() with the exception of EINTR handled
FILE* open_log_(const char *path, const char *mode) ;

// This function's purpose lies solely in creating the timestamp
// Prints a two - digit number to the dest ; If the number has only 1 digit, 0 is appended to the front.
// For numbers > 99 (more than 2 digits), the lowest two digits are observed
// Arguments:
//		char *dest: destination into which we write the string
//		int val: the value that is to be converted to the string
// Returns:
//		nothing (return type void)
void print_small_(char *dest, int val) ;

//Returns a formatted timeprint or NULL, if unsuccessful
//Format: [HH:MM:SS DD-MM-YYY TMZN] where TMZN is not of fixed lentgh and everything else is
// Arguments:
//		none
//	Returns:
//		A string in the above described format
char* get_time_() ;

// Prints a message msg to the file stream on path logpath. Opens and closes the path itself.
// Also appends a timestamp obtained using the function get_time_()
// Arguments:
//		const char* msg: The message that is to be logged
//		const char* logpath: The path of the file the messaged is to be logged in
//		const char* timestamp: String representing the timestamp
//	Returns:
//		An indicator if everything is okay. See the possible return values above
int log_msg_(const char* msg, const char* timestamp, const char* logpath) ;

// Just a wrapper around the above function
// Uses get_time() as timestamp argument
int log_msg_now_(const char* msg, const char* logpath) ;

// Append data buffer to the file with the path logpath. A header is written before the data, uniquely detemrining
// the time of the writing and the size of data
// The first time_t bytes of the record written contain the time of the creation of the record
// and the size_t bytes after that contain the length of the rest of the record
// Therefore, the record written is time_t + size_t + sizeof(unsigned char)*length bytes long
// Notice that sizeof(unsigned char) is 1 but this just seemed a more elegant way to write it.
// Agruments:
//		const char *data: the sequence of bytes to be read
//		const size_t length: the length of the abovementioned sequence
// 	const char* logpath: the path to the log file
//	Returns:
//		An indicator if everything is okay. See the possible return values above.
int log_bin_(const char *data, const size_t length, const char* logpath) ;

// Remove the last record from the binary file and return its contents (as a binary buffer);
// The first time_t bytes of the logpath buffer contain the time of the creation of the record
// and the size_t bytes after that contain the length of the rest of the buffer
// the return buffer is allocated inside the function and, when possible, always fits the size of the header + data perfectly
// The file is then truncated to remove the unlogged message.
//	Arguments:
//		const char* logpath: the path to the log file
//		int *err: an indicator if everything is okay. See the possible return values above.
//	Returns:
//		The read sequence, starting with the header, if everything is successful. NULL otherwise.
char* read_bin_full_data_(const char* logpath, int *err) ;

// This is a wrapper for unlog_bin_full_data_(), which receives the path, the address of the variable to write error to, the address
// of the time_t variable to write the time part of the header and the size_t variable to write the size part of the header to,
// meaning that the user is not obliged to do the parsing of the data if he/she doesn't want to
//	Arguments:
//		const char* logpath: the path to the log file
//		int *err: An indicator if everything is okay. See the possible return values above.
//		time_t *t: The read time structure that indicates the time of the logging of the read data.
//		size_t *s: The read size_t variable that holds the size of the read data, in bytes.
char* read_bin_parsed_(const char* logpath, int *err, time_t *t, size_t *s) ;

//	Made to truncate file by the specified path by amount bytes
//	Arguments:
//		const char *path: The path of the file we are truncating
//		unsigned amount: The amount of bytes that are to be removed from the end of file
//	Returns:
//		An indicator if everything is okay. See the possible return values above. (WRITE_FAIL_ is used to indicate truncate() call failure)
int truncate_by_(const char *path, unsigned amount) ;

// Wrap around the truncate_by_() call using LAST_READ_SIZE_ as the amount argument
// Also resets LAST_READ_SIZE_ to 0 if (and only if) the truncate_by_() call was successful
// Arguments:
//		const char *path: The path of the file we are truncating
//	Returns:
//		An indicator if everything is okay. See the possible return values above. (WRITE_FAIL_ is used to indicate truncate() call failure)
int truncate_last_(const char *path) ;
#endif
