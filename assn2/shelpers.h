#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

char * getIPString(struct sockaddr * client) {
	char *buff;
	buff = malloc(sizeof(char)*20);
	if (buff == NULL)
		return NULL;
	
	sprintf(buff, "%d.%d.%d.%d",
		(int)(client->sin_addr.s_addr&0xFF),
		(int)((client->sin_addr.s_addr&0xFF00)>>8),
		(int)((client->sin_addr.s_addr&0xFF0000)>>16),
		(int)((client->sin_addr.s_addr&0xFF000000)>>24));
	
	return buff;
}
void getTime(char * buffer) {
	time_t t = time(NULL);	
	strftime(buffer, 80,"%a, %d %b %Y %X GMT", gmtime(&t));
}
int checkGET(char * buff, char * fileName, char * firstLine) {
	char *word, *line, *lptr, *wptr;

	wptr = NULL;
	lptr = NULL;	
	
	/* First line should be 'GET /someplace/file.html HTTP/1.1' */
	
	line = strtok_r(buff, "\n", &lptr);
	strlcpy(firstLine, line);

	int i;
	
	word = strtok_r(line, " ", &wptr);
	if (word == NULL || strncmp(word, "GET", 3) != 0) {
		free(buff);
		return 0;
	}
	
	word = strtok_r(NULL, " ", &wptr);
	if (word == NULL) {
		free(buff);
		return 0;
	}
	strlcpy(fileName, word);
	if (fileName[0] != '/') {
		free(buff);
		return 0;
	}
	memmove(fileName, fileName+1, strlen(fileName+1));
	fileName[strlen(fileName)-1] = '\0';
	
	word = strtok_r(NULL, " ", &wptr);
	if (word == NULL || strncmp(word, "HTTP/1.1",  8) != 0) {
		free(buff);
		return 0;	
	}
	
	/* Ensure there is a blank line */
	while (line != NULL) {
		if (strlen(line) == 1) {
			free(buff);
			return 1;
		}
		line = strtok_r(NULL, "\n", &lptr);
	}
	free(buff);
	return 0;
}
