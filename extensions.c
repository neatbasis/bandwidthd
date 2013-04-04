#include "bandwidthd.h"
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

extern struct config config;

static char *execute_extension(char * filename);

struct extensions *execute_extensions(void)
	{
	struct dirent **namelist;
	int Counter, n;
	struct stat statbuffer;
	char *filename;	
	struct extensions *results = NULL;
	struct extensions **resptr;
	char *ptr;

	resptr = &results;

	n = scandir(EXTENSION_DIR, &namelist, 0, alphasort);
	if (n < 0)
		syslog(LOG_ERR, "Failed to open %s for executing extensions", EXTENSION_DIR);
	else 
		{
		for(Counter = 0; Counter < n; Counter++) 
			{
			filename = malloc(strlen(EXTENSION_DIR)+strlen(namelist[Counter]->d_name)+2);
			sprintf(filename, "%s/%s", EXTENSION_DIR, namelist[Counter]->d_name);
			stat(filename, &statbuffer);
			
			if (S_ISREG(statbuffer.st_mode))
				{
				ptr = execute_extension(filename);
				if (ptr)
					{
					*resptr = malloc(sizeof(struct extensions));
					(*resptr)->name = strdup(namelist[Counter]->d_name);
					(*resptr)->value = ptr;			
					(*resptr)->next = NULL;
					resptr = &((*resptr)->next);
					}
				}
			free(filename);
			free(namelist[Counter]);
			}
		free(namelist);
		}

	return(results);
	}

static char *execute_extension(char *filename)
	{
	int ExtPipe[2];
	pid_t extpid;
	char buffer[1500];
	char *res = NULL;
	char *ptr;
	ssize_t len;
	int child;
	int Counter;

	fd_set rset;
	fd_set eset;

	if (pipe(ExtPipe))
		{
		syslog(LOG_ERR, "Failed to open pipe for extension processing");
		return(NULL);
		}

	if (!(extpid = fork()))
		{
		close(ExtPipe[0]);
		dup2(ExtPipe[1], 1);

		execl(filename, filename, config.dev, NULL);
		syslog(LOG_ERR, "Failed to launch extension %s", filename);
		_exit(1);
		}

	close(ExtPipe[1]);
	
	child = 1;
	while (child)
		{
		child = !waitpid(extpid, NULL, WNOHANG);
	
		// If child is running drop into a select and wait for data
		if (child)
			{
			FD_ZERO(&rset);
			FD_SET(ExtPipe[0], &rset);
			FD_ZERO(&eset);
			FD_SET(ExtPipe[0], &eset);
			
			select(ExtPipe[0]+1, &rset, NULL, &eset, NULL);
			}
		
		len = 1;
		while (len)
			{
			len = read(ExtPipe[0], buffer, 1499);  
			buffer[len] = '\0';

			if (res)
				{
				ptr = malloc(strlen(res)+len+1);
				sprintf(ptr, "%s%s", res, buffer);
				free(res);
				}
			else
				{
				ptr = malloc(len+1);
				sprintf(ptr, "%s", buffer);
				}
			res = ptr;
			}
		}
	close(ExtPipe[0]);

	// Trim whitespace from end
	for(Counter = strlen(res)-1; Counter >= 0; Counter--)
		{
		if (res[Counter] == ' ' || res[Counter] == '\t' || res[Counter] == '\r' || res[Counter] == '\n')
			res[Counter] = '\0';
		else
			break;
		}

	// Filter empty responses (Also filters all whitespace responses because of previouse trim)
	if (strlen(res) < 1)
		{
		free(res);
		return(NULL);
		}
	else
		return(res);
	}

void destroy_extension_data(struct extensions *ext)
	{
	struct extensions *ptr;
	struct extensions *ptr2;
	ptr = ext;
	while(ptr)
		{
		if (ptr->name) 
			free(ptr->name);
		if (ptr->value)
			free(ptr->value);
		ptr2 = ptr->next;
		free(ptr);
		ptr = ptr2;
		}
	}

/*
int main(void)
	{
	struct extensions *res;
	struct extensions *resptr;

	res = execute_extensions();
	resptr = res;

	while(resptr)
		{
		printf("%s\n", resptr->name);
		printf("%s\n", resptr->value);
		resptr = resptr->next;
		}

	destroy_extensions(res);
	return(0);
	}

*/
