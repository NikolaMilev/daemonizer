#include "logger.h"

char LOGPATH_[PATH_LEN_] = "log.txt";
char LOGERRPATH_[PATH_LEN_] = "err.txt";
char LOGBINPATH_[PATH_LEN_] = "logbin.txt";

FILE *logFile = NULL ;


void free_fp_(FILE **fp)
{
	fclose(*fp);
	*(fp) = NULL;
}

void logger_cleanup_()
{
	if(logFile != NULL)
	{
		free_fp_(&logFile);
	}
}

void print_small_(char *dest, int val)
{
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

	time_now = time(NULL);
	timestruct = localtime(&time_now);

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

int log_msg_ (const char* msg, const char* logpath)
{
	char newline[3];
	int indicator;
	char* get_time_val;
	newline[0]='\0';
	newline[1]='\0';
	newline[2]='\0';

	//Try to open
	if((logFile = fopen(logpath, "a+")) == NULL)
	{
		return -1;
	}

	//Check if there is need for new line or not and which one to insert
	if(SEPARATE_BY_NEWLINE_ == 1)
	{
		newline[0] = '\n';
		if(strlen(NEWLINE_STRING_) == 2)
		{
			newline[1] = '\r';
		}
	}

	get_time_val = get_time_();
	indicator = fprintf(logFile, "%s%s%s", get_time_val, msg, newline);
	if(indicator < 0)
	{
		indicator = -1;
	}
	else if(indicator < strlen(msg) + strlen(newline) + TIMESTAMP_LEN_)
	{
		indicator = -2;
	}
	else
	{
		indicator = 0;
	}

	free(get_time_val);
	free_fp_(&logFile);
	return indicator;
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


	//Lower indices mean lower addresses in the number (on my current PC, needs to be checked in the openwrt)
	//I will read and write in the same order so this should not be a problem
	//For instance:
	//	TIME: 1471512034	 LENGTH: 3
	//	Printed bytes:
	//	226 125 181 87 0 0 0 0 3 0 0 0 0 0 0 0

	//Filling the header
	for(i = 0; i<sizeof(time_t); i++)
	{
		header[i] = tmstr.char_f[i];
	}
	for(i = 0; i<sizeof(size_t); i++)
	{
		header[i + sizeof(time_t)] = szstr.char_f[i];
	}

	//a+b means that everything I write is appended to the end and that the file is read in binary mode (byte by byte)
	if((logFile = fopen(logpath, "a+b")) == NULL)
	{
		return -1;
	}

	header_bytesout_ = fwrite(header, sizeof(char), HEADER_LEN_, logFile);
	data_bytesout_ = fwrite(data, sizeof(char), length, logFile);

	free_fp_(&logFile);
	if(header_bytesout_ <= 0 || data_bytesout_ < 0)
	{
		//maybe a cleanup: remember the position of the file before the operations and return there
		return -1;
	}

	return header_bytesout_ + data_bytesout_ ;
}

char* unlog_bin_full_data_(const char* logpath, int *err)
{
	char *ret_buffer ;
	char tmp;
	time_str_ tmstr;
	size_str_ szstr;
	int ctr;

	*err = 0;
	tmstr.time_f = 0;
	szstr.size_f = 0;

	//Opening the file in append/write mode, as a binary file
	if((logFile = fopen(logpath, "a+b")) == NULL)
	{
		*err = -2;
		return NULL;
	}

	//Checking if the file is empty; SEEK_END is the position of the end of the file and SEEK_SET is the position of the start of the file
	if(fseek(logFile, 0, SEEK_END) - fseek(logFile, 0, SEEK_SET))
	{
		//This means the file is empty
		free_fp_(&logFile);
		return NULL;
	}

	//The main loop of the function
	while(1)
	{
		//If we can't read a byte, it means we reached the end of the file or an error occurred
		if(fread(&tmp, sizeof(char), 1, logFile) != 1)
		{
			//feof(logFile) will return a non-zero value if we attempted to read after reaching the end of the file
			if(feof(logFile))
			{


				long pos;

				//Allocating the return buffer
				ret_buffer = malloc(HEADER_LEN_ + szstr.size_f);
				if(ret_buffer == NULL)
				{
					*err = -1;
					free_fp_(&logFile);
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
					*err = -3;
					free_fp_(&logFile);
					return NULL;
				}

				//Again moving backwards, so that we can truncate the file
				fseek(logFile, -(szstr.size_f+ HEADER_LEN_), SEEK_END);
				pos = ftell(logFile);
				truncate(logpath, pos);
				break;
			}
			else
			{
				//An error occurred, do a cleanup and return error
				free_fp_(&logFile);
				*err = -3;
				return NULL;
			}
		}
		else
		{
			//Going one back
			fseek(logFile, -sizeof(unsigned char), SEEK_CUR);
			// printf("FSEEK %d\n", );
			// printf("ERRNO %d\n", errno);

			//Reading the time part of the header into the structure tmstr
			for(ctr = 0; ctr < sizeof(time_t); ctr++)
			{
				if(fread(&tmp, sizeof(unsigned char), 1, logFile) != 1)
				{
					//An error or bad file structure
					free_fp_(&logFile);
					*err = -3;
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
					free_fp_(&logFile);
					*err = -3;
					return NULL;
				}
				szstr.char_f[ctr] = tmp;
			}
			//Moving forward, skipping the data
			fseek(logFile, szstr.size_f, SEEK_CUR);
		}
	}
	free_fp_(&logFile);
	return ret_buffer;
}

char* unlog_bin_parsed_(const char* logpath, int *err, time_t *t, size_t *s)
{
	char* data ;
	int i;
	char *ret_buffer;
	time_str_ tmstr;
	size_str_ szstr;

	if(logpath == NULL)
	{
		*err = -4;
		return NULL;
	}

	data = unlog_bin_full_data_(logpath, err);

	if(data == NULL)
	{
		return NULL;
	}

	for(i=0; i < sizeof(time_t); i++)
	{
		tmstr.char_f[i] = data[i];
	}
	for(i=0; i < sizeof(size_t); i++)
	{
		szstr.char_f[i] = data[i + sizeof(time_t)];
	}

	*t = tmstr.time_f;
	*s = szstr.size_f;

	ret_buffer = malloc(*s);

	if(ret_buffer == NULL)
	{
		*err = -1;
		return NULL;
	}

	strncpy(ret_buffer, data + HEADER_LEN_, *s);

	free(data);
	return ret_buffer;
}
