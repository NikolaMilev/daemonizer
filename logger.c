#include "logger.h"
#include <errno.h>

char LOGPATH_[PATH_LEN_] = "log.txt";
char LOGERRPATH_[PATH_LEN_] = "err.txt";
char LOGBINPATH_[PATH_LEN_] = "logbin.txt";
char SEPARATE_BY_NEWLINE_ = 1;
char NEWLINE_STRING_[3] = "\n";
size_t LAST_READ_SIZE_BIN_  = 0;

FILE *logFile = NULL ;

void logger_cleanup_()
{
	if(logFile != NULL)
	{
		fclose(logFile);
		logFile = NULL;
	}
}

FILE* open_log_(const char *path, const char *mode)
{
	FILE *fp;
	//Try to open the file; repeating if the open() call was interrupted by a signal
	while(1)
	{
		if((fp = fopen(path, mode)) == NULL)
		{
			if(errno == EINTR)
			{
				continue;
			}
		}
		break;
	}

	return fp;
}

void print_small_(char *dest, int val)
{
	// This function bears no significance on the logic of the logging system
	// itself; It is merely a tool for writing seconds, minutes or hours using
	// exactly 2 digits
	val = val % 100 ;
	if(val < 10)
	{
		dest[0] = '0';
		dest[1] = (char)(val + '0');
	}
	else
	{
		dest[0] = (char)(val / 10 + '0');
		dest[1] = (char)(val % 10 + '0');
	}
	dest[2] = '\0';
}

char* get_time_()
{
	time_t time_now;
	char *buf;
	//Pointers returned by localtime are pointer sto statically allocated memory and, as such, MUST NOT be freed
	struct tm  *timestruct;

	// More on time structures is to be found on man pages or online
	time_now = time(NULL);
	timestruct = localtime(&time_now);

	// If an error was detected, we return NULL
	if(timestruct == NULL)
	{
		return NULL ;
	}
	else
	{
		size_t msg_len = TIMESTAMP_LEN_ + 1;
		buf = malloc(msg_len);
		if(buf == NULL)
		{
			return NULL ;
		}
		else
		{
			char hour[3], min[3], sec[3], day[3], month[3];
			int year = timestruct->tm_year + ORIGIN_YEAR_ ;
			print_small_(hour, timestruct->tm_hour);
			print_small_(min, timestruct->tm_min);
			print_small_(sec, timestruct->tm_sec);
			print_small_(day, timestruct->tm_mday);
			print_small_(month, timestruct->tm_mon + 1);

			snprintf(buf, TIMESTAMP_LEN_ + strlen(timestruct->tm_zone), "%c%s%c%s%c%s%c%s%c%s%c%4d%c%s%c", TIMEDELIM_START_, hour, TIMEDELIM_TIME_, min, TIMEDELIM_TIME_,
				sec, TIMEDELIM_DATE_TIME_, day, TIMEDELIM_DATE_, month, TIMEDELIM_DATE_, year, TIMEDELIM_DATE_TIME_, timestruct->tm_zone , TIMEDELIM_END_);
			return buf;
		}
	}
	return buf;
}

int log_msg_(const char* msg, const char* timestamp, const char* logpath)
{
	char newline[3];
	int indicator;
	char* get_time_val;
	newline[0]='\0';
	newline[1]='\0';
	newline[2]='\0';

	//Try to open
	if((logFile = open_log_(logpath, "a+")) == NULL)
	{
		return OPEN_FAIL_;
	}

	//Check if there is need for new line or not and copy it
	if(SEPARATE_BY_NEWLINE_ == 1)
	{
		strcpy(newline, NEWLINE_STRING_);
	}

	if(timestamp == NULL || timestamp[0] == '\0')
	{
		get_time_val = get_time_();
	}
	else
	{
		get_time_val = (char*)timestamp;
	}
	indicator = fprintf(logFile, "%s%c%s%c%s", get_time_val, MSGDELIM_START_, msg, MSGDELIM_END_, newline);
	//This means fprintf failed
	if(indicator < 0)
	{
		fclose(logFile);
		return WRITE_FAIL_;
	}
	//If written less than expected, deleting all that's been written
	//Everything is written or nothing is
	//Not sure this is even a possible situation but want to be sure
	else if(indicator < strlen(msg) + strlen(newline) + TIMESTAMP_LEN_)
	{
		unsigned long pos;
		fseek(logFile, 0, SEEK_END);
		pos = ftell(logFile);
		fclose(logFile);
		// Since the calls are blocking, we try again if interrupted by a signal
		while(1)
		{
			truncate(logpath, pos - indicator);
			if(errno == EINTR)
			{
				continue;
			}
			break;
		}
		return WRITE_FAIL_;
	}
	//Everything is fine
	else
	{
		fclose(logFile);
		return OK_;
	}
}

int log_msg_now_(const char* msg, const char* logpath)
{
	return log_msg_(msg, get_time_(), logpath);
}

int log_bin_(const char *data, const size_t length, const char* logpath)
{
	char header[HEADER_LEN_];
	time_str_ tmstr;
	size_str_ szstr;
	int i;
	size_t header_bytesout_, data_bytesout_;
	tmstr.time_f = time(NULL);
	szstr.size_f = length;

	// Lower indices mean lower addresses in the number (on my current PC, needs to be checked in the openwrt)
	// I will read and write in the same order so this should not be a problem
	// For instance:
	//	TIME: 1471512034	 LENGTH: 3
	//	Printed bytes:
	//	226 125 181 87 0 0 0 0 3 0 0 0 0 0 0 0

	// Filling the header
	for(i = 0; i<sizeof(time_t); i++)
	{
		header[i] = tmstr.char_f[i];
	}
	for(i = 0; i<sizeof(size_t); i++)
	{
		header[i + sizeof(time_t)] = szstr.char_f[i];
	}

	//	Opening the file we are logging to
	// a+b means that everything I write is appended to the end and that the file is interpreted in binary mode (byte by byte)
	if((logFile = open_log_(logpath, "a+b")) == NULL)
	{
		return OPEN_FAIL_;
	}

	header_bytesout_ = fwrite(header, sizeof(char), HEADER_LEN_, logFile);

	// fwrite returns number of items written (with sizeof(char), number of characters)
	// if the number is zero or less than HEADER_LEN_, we can assume that the writing is unsuccessful
	if(header_bytesout_ == 0)
	{
		fclose(logFile);
		return WRITE_FAIL_;
	}
	else if(header_bytesout_ < HEADER_LEN_)
	{
		unsigned long pos;
		fseek(logFile, 0, SEEK_END);
		pos = ftell(logFile);
		fclose(logFile);
		// Since the calls are blocking, we try again if interrupted by a signal
		while(1)
		{
			truncate(logpath, pos - header_bytesout_);
			if(errno == EINTR)
			{
				continue;
			}
			break;
		}
		return WRITE_FAIL_;
	}

	data_bytesout_ = fwrite(data, sizeof(char), length, logFile);
	//The same thing as above but we also delete the header from the file
	//because it's a part of the same logged piece of information
	if(data_bytesout_ == 0)
	{
		fclose(logFile);
		return WRITE_FAIL_;
	}
	else if(data_bytesout_ < length)
	{
		unsigned long pos;
		fseek(logFile, 0, SEEK_END);
		pos = ftell(logFile);
		fclose(logFile);
		// Since the calls are blocking, we try again if interrupted by a signal
		while(1)
		{
			truncate(logpath, pos - header_bytesout_ - data_bytesout_);
			if(errno == EINTR)
			{
				continue;
			}
			break;
		}
		return WRITE_FAIL_;
	}

	fclose(logFile);
	return header_bytesout_ + data_bytesout_ ;
}

char* read_bin_full_data_(const char* logpath, int *err)
{
	char *ret_buffer ;
	char tmp;
	time_str_ tmstr;
	size_str_ szstr;
	int ctr;

	// Assigning the variables default values
	*err = OK_;
	tmstr.time_f = 0;
	szstr.size_f = 0;

	//Opening the file in append/write mode, as a binary file
	if((logFile = open_log_(logpath, "a+b")) == NULL)
	{
		*err = OPEN_FAIL_;
		return NULL;
	}

	// Checking if the file is empty; SEEK_END is the position of the end of the file and thus also the size of file in bytes
	fseek(logFile, 0, SEEK_END);
	if(ftell(logFile) == 0)
	{
		//This means the file is empty; there is nothing to be read
		fclose(logFile);
		return NULL;
	}
	//Returning to the beginning of the file
	fseek(logFile, 0, SEEK_SET);

	//The main loop of the function
	while(1)
	{
		// If we can't read a byte, it means we reached the end of the file or an error occurred
		if(fread(&tmp, sizeof(char), 1, logFile) != 1)
		{
			// feof(logFile) will return a non-zero value if we attempted to read after reaching the end of the file
			if(feof(logFile))
			{
				// Allocating the return buffer
				ret_buffer = malloc(HEADER_LEN_ + szstr.size_f);
				if(ret_buffer == NULL)
				{
					*err = MALLOC_FAIL_;
					fclose(logFile);
					return NULL;
				}

				//Filling the return buffer with the header
				strncpy(ret_buffer, tmstr.char_f, sizeof(time_t));
				strncpy(ret_buffer + sizeof(time_t), szstr.char_f, sizeof(size_t));

				//Return back the number of bytes corresponding to the second part of the header
				fseek(logFile, -(szstr.size_f), SEEK_END);

				//Reading the data into the rest of the return buffer
				if(fread((ret_buffer + HEADER_LEN_), sizeof(unsigned char), szstr.size_f * sizeof(unsigned char), logFile) != szstr.size_f)
				{
					*err = READ_FAIL_;
					fclose(logFile);
					return NULL;
				}

				// Breaking out of the loop so we can return successfully;
				// Remembering the last read size
				LAST_READ_SIZE_BIN_ = szstr.size_f ;
				break;
			}
			//An error occurred, do a cleanup and return error
			else
			{
				fclose(logFile);
				*err = READ_FAIL_;
				return NULL;
			}
		}
		// We have read one byte successfully. That was just a test so we have to move one byte back
		else
		{
			//Going one byte back
			fseek(logFile, -sizeof(unsigned char), SEEK_CUR);

			//Reading the time part of the header into the structure tmstr
			for(ctr = 0; ctr < sizeof(time_t); ctr++)
			{
				if(fread(&tmp, sizeof(unsigned char), 1, logFile) != 1)
				{
					//An error or bad file structure
					fclose(logFile);
					*err = READ_FAIL_;
					return NULL;
				}
				tmstr.char_f[ctr] = tmp;
			}

			//Reading the size part of the header into the structure szstr
			for(ctr = 0; ctr < sizeof(size_t); ctr++)
			{
				if(fread(&tmp, sizeof(unsigned char), 1, logFile) != 1)
				{
					//The same error as above
					//I assume it's my structure which is good for this kind of read
					fclose(logFile);
					*err = READ_FAIL_;
					return NULL;
				}
				szstr.char_f[ctr] = tmp;
			}
			// Moving forward, skipping the data;
			// We have saved the last read size and time of logging into the structures
			// and when we reach the end of file, we will know how many bytes to return
			fseek(logFile, szstr.size_f, SEEK_CUR);
		}
	}
	fclose(logFile);
	return ret_buffer;
}

char* read_bin_parsed_(const char* logpath, int *err, time_t *t, size_t *s)
{
	char* data ;
	int i;
	char *ret_buffer;
	time_str_ tmstr;
	size_str_ szstr;

	// Checking if the path is NULL or not

	if(logpath == NULL)
	{
		*err = BAD_LOG_PATH_;
		return NULL;
	}

	data = read_bin_full_data_(logpath, err);

	// The potential error is recorded in err variable so it's safe to just return NULL
	if(data == NULL)
	{
		return NULL;
	}

	// Copying the header into the structures
	for(i=0; i < sizeof(time_t); i++)
	{
		tmstr.char_f[i] = data[i];
	}
	for(i=0; i < sizeof(size_t); i++)
	{
		szstr.char_f[i] = data[i + sizeof(time_t)];
	}

	// Copying the values from the structures into the time_t and size_t variables, respectively
	*t = tmstr.time_f;
	*s = szstr.size_f;

	ret_buffer = malloc(*s);
	if(ret_buffer == NULL)
	{
		*err = MALLOC_FAIL_;
		return NULL;
	}
	//	Filling the return buffer and returning the data acquired
	strncpy(ret_buffer, data + HEADER_LEN_, *s);

	free(data);
	return ret_buffer;
}

int truncate_by_(const char *path, unsigned amount)
{
	unsigned long pos;
	FILE* f;

	//Opening the file for read so that we can find the position of the end of the file
	if((f = open_log_(path, "rb")) == NULL)
	{
		return OPEN_FAIL_;
	}

	// Obtaining the end position of the file. The end position is the length
	// of the file, in bytes
	fseek(f, 0, SEEK_END);
	pos = ftell(f);

	// We no longer need the open file
	fclose(f);

	// Repeating while interrupted by a signal
	while(1)
	{
		if(truncate(path, pos - amount) < 0)
		{
			if(errno == EINTR)
			{
				continue;
			}
			else
			{
				return WRITE_FAIL_;
			}
		}
		else
		{
			return OK_;
		}
	}
}

int truncate_last_bin_(const char *path)
{
	int indicator;
	indicator = truncate_by_(path, LAST_READ_SIZE_BIN_);
	// If everything went okay with truncate_by_() call, we reset LAST_READ_SIZE_BIN_  and return OK_
	if(indicator == OK_)
	{
		LAST_READ_SIZE_BIN_ = 0;
		return OK_;
	}

	// Otherwise, failure
	return indicator;
}
