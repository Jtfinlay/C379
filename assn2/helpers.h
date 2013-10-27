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
	
char * LOG_FILE;
	
char * getIPString(struct sockaddr_in * client) {
	char *buff, *tmp;
	tmp = malloc(sizeof(char)*20);
	if (tmp == NULL)
		return NULL;
	buff = tmp;
	
	sprintf(buff, "%d.%d.%d.%d",
		(int)(client->sin_addr.s_addr&0xFF),
		(int)((client->sin_addr.s_addr&0xFF00)>>8),
		(int)((client->sin_addr.s_addr&0xFF0000)>>16),
		(int)((client->sin_addr.s_addr&0xFF000000)>>24));
		
	tmp = NULL;
	return buff;
}

void getTime(char * buffer) {
	time_t t = time(NULL);
	
	strftime(buffer, 80,"%a, %d %b %Y %X GMT", gmtime(&t));
	
}
void internalError(struct sockaddr * client, char * error, char * get) {
	char * ip;
	sendGenError(client);
	
	if (get != NULL)
		logInternal(getIPString(client), get);
	err(1, error);
}
/* Returns characters sent */
int writeToClient(int clientsd, char * buff) {
	ssize_t w, written;

	w = 0;
	written = 0;
	while (written < strlen(buff)) {
		w = write(clientsd, buff + written,
			strlen(buff) - written);
		if (w == -1) {
			if (errno != EINTR)
				return written;
		} else
			written += w;
	}
	return written;
}
int checkGET(char * buff, char * fileName, char * firstLine) {
	char *backup, *word, *line, *lptr, *wptr;

	wptr = NULL;
	lptr = NULL;	
	
	/* First line should be 'GET /someplace/file.html HTTP/1.1' */
	//strlcpy(backup, buff);
	backup = strndup(buff, strlen(buff));
	if (backup == NULL)
		err(0, "strndup fail");
	
	line = strtok_r(backup, "\n", &lptr);
	strlcpy(firstLine, line);

	int i;
	
	word = strtok_r(line, " ", &wptr);
	if (word == NULL || strncmp(word, "GET", 3) != 0) {
		free(backup);
		return 0;
	}
	
	word = strtok_r(NULL, " ", &wptr);
	if (word == NULL) {
		free(backup);
		return 0;
	}
	strlcpy(fileName, word);
	if (fileName[0] != '/') {
		free(backup);
		return 0;
	}
	fileName[strlen(fileName)+1] = '\0';
	
	word = strtok_r(NULL, " ", &wptr);
	if (word == NULL || strncmp(word, "HTTP/1.1",  8) != 0) {
		free(backup);
		return 0;	
	}
	
	/* Ensure there is a blank line */
	printf("Ensure blank line.\n");
	line = strtok_r(NULL, "\n", &lptr);
	while (line != NULL) {
		printf("line : %d\n", strlen(line));
		if (strlen(line) == 1) {
			free(backup);
			printf("blank line!\n");
			return 1;
		}
		line = strtok_r(NULL, "\n", &lptr);
	}
	printf("No blank.\n");
	free(backup);
	return 0;
}
void sendOK(int clientsd, int fileLen) {
	char buf[128], length[20], time[80];
	
	sprintf(length, "%d", fileLen);
	
	getTime(time);

	strlcpy(buf, "HTTP/1.1 200 OK\n");
	strcat(buf, "Date: ");
	strcat(buf, time);
	strcat(buf, "\nContent-Type: text/html\n");
	strcat(buf, "Content-Length: ");
	strcat(buf, length);
	strcat(buf, "\n\n");
	
	writeToClient(clientsd, buf);
}
void sendError(int clientsd, char * title, char * content) {
	char *buf, *tmp, length[20], time[80];
	tmp = (char*) malloc((128+strlen(content))*sizeof(char));
	if (tmp == NULL)
		err(1, "malloc");
	
	buf = tmp;
	getTime(time);
	
	sprintf(length, "%d", strlen(content));
	
	strlcpy(buf, "HTTP/1.1 ");
	strcat(buf, title);
	strcat(buf, "Date: ");
	strcat(buf, time);
	strcat(buf, "\nContent-Type: text/html\n");
	strcat(buf, "Content-Length: ");
	strcat(buf, length);
	strcat(buf, "\n\n");
	strcat(buf, content);
	
	writeToClient(clientsd, buf);
	
	free(buf);
	buf=NULL;
	tmp=NULL;
}
void sendBadRequestError(int clientsd) {
	char title[30], content[128];
	
	strlcpy(title, "400 Bad Request\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Malformed Request</h2>\n");
	strcat(content, "Your browser sent a request I could not understand\n");
	strcat(content, "</body></html>\n");
	
	sendError(clientsd, title, content);
}
void sendNotFoundError(int clientsd) {
	char title[30], content[128];
	
	strlcpy(title, "404 Not Found\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Document not found</h2>\n");
	strcat(content, "You asked for a document that doesn't exist.\n");
	strcat(content, "</body></html>\n");
	
	sendError(clientsd, title, content);
}
void sendForbiddenError(int clientsd) {
	char title[30], content[128];
	
	strlcpy(title, "403 Forbidden\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Permission denied</h2>\n");
	strcat(content, "You cannot see this document.\n");
	strcat(content, "</body></html>\n");
	
	sendError(clientsd, title, content);
}
void sendGenError(int clientsd) {
	char title[30], content[128];
	
	strlcpy(title, "500 Internal Server Error\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Oops. That didn't work.</h2>\n");
	strcat(content, "Something bad happened.\n");
	strcat(content, "</body></html>\n");	

	sendError(clientsd, title, content);
}
int sendFile(FILE * fp, int clientsd) {
	int buffSize = 256;
	int counter;
	char buff[buffSize+1];
	int r;
	
	memset(buff, '\0', (buffSize+1)*sizeof(char));
	r = buffSize;
	counter = 0;
	while (r == buffSize) {
		r = fread(buff, sizeof(char), buffSize, fp);
		counter += writeToClient(clientsd, buff);
		
		memset(buff, '\0', buffSize*sizeof(char));
	}		
	return counter;	
}






void writeLog(char * ip, char * get, char * req) {
	char buf[150+strlen(get)];
	FILE *f;

	//strlcpy(buf, getTime());
	getTime(buf);
	strcat(buf, "\t");
	strcat(buf, ip);
	strcat(buf, "\t");
	strncat(buf, get, strlen(get)-1);
	strcat(buf, "\t");
	strcat(buf, req);
	strcat(buf, "\n");
	
	f = fopen(LOG_FILE, "a");
	if (f == NULL)
		err(0, "log fopen");
	fputs(buf, f);
	fclose(f);
	
	free(ip);
	
}
void logOK(char * ip, char * get, int iWrote, int iTotal) {
	char buf[40];
	
	sprintf(buf, "200 OK %d/%d", iWrote, iTotal); 
	writeLog(ip, get, buf);
}
void logBadRequest(char * ip, char * get) {
	writeLog(ip, get, "400 Bad Request");
}
void logNotFound(char * ip, char * get) {
	writeLog(ip, get, "404 Not Found");
}
void logForbidden(char * ip, char * get) {
	writeLog(ip, get, "403 Forbidden");
}
void logInternal(char * ip, char * get) {
	writeLog(ip, get, "500 Internal Server Error");
}
