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

/* we use this structure to keep track of each connection to us */
struct con {
	int sd; 	/* the socket for this connection */
	int state; 	/* the state of the connection */
	struct sockaddr_in sa; /* the sockaddr of the connection */
	size_t  slen;   /* the sockaddr length of the connection */
	char *buf;	/* a buffer to store the characters read in */
	char *bp;	/* where we are in the buffer */
	size_t bs;	/* total size of the buffer */
	size_t bl;	/* how much we have left to read/write */
};

char * getIPString(struct sockaddr_in * client) {
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
	memmove(fileName, fileName+1, strlen(fileName+1));
	fileName[strlen(fileName)-1] = '\0';
	
	word = strtok_r(NULL, " ", &wptr);
	if (word == NULL || strncmp(word, "HTTP/1.1",  8) != 0) {
		free(backup);
		return 0;	
	}
	
	/* Ensure there is a blank line */
	while (line != NULL) {
		if (strlen(line) == 1) {
			free(backup);
			return 1;
		}
		line = strtok_r(NULL, "\n", &lptr);
	}
	free(backup);
	return 0;
}
void sendError(struct con *cp, char * title, char * content) {
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
	
	writeToClient(cp, buf);
	
	free(buf);
	buf=NULL;
	tmp=NULL;
}
void sendBadRequestError(struct con *cp) {
	char title[30], content[128];
	
	printf("Bad Request Error\n");
	
	strlcpy(title, "400 Bad Request\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Malformed Request</h2>\n");
	strcat(content, "Your browser sent a request I could not understand\n");
	strcat(content, "</body></html>\n");
	
	sendError(c, title, content);
}
void sendNotFoundError(int clientsd) {
	char title[30], content[128];
	
	printf("Not Found Error\n");
	
	strlcpy(title, "404 Not Found\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Document not found</h2>\n");
	strcat(content, "You asked for a document that doesn't exist.\n");
	strcat(content, "</body></html>\n");
	
	sendError(clientsd, title, content);
}
void sendForbiddenError(int clientsd) {
	char title[30], content[128];
	
	printf("Forbidden Error\n");
	
	strlcpy(title, "403 Forbidden\n");
	strlcpy(content, "<html><body>\n");
	strcat(content, "<h2>Permission denied</h2>\n");
	strcat(content, "You cannot see this document.\n");
	strcat(content, "</body></html>\n");
	
	sendError(clientsd, title, content);
}
void sendGenError(int clientsd) {
	char title[30], content[128];
	
	printf("Generic Error\n");
	
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











