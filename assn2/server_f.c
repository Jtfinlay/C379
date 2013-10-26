/*
 * Licensing goes here.
 *
 */

/*
 * Compile using something
 */

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

#ifndef HELPERS_H
#define HELPERS_H
	#include "helpers.h"
#endif

static void usage()
{
	extern char * __progname;
	fprintf(stderr, "usage: %s portnumber dir logfile\n", __progname);
	exit(1);
}
static void kidhandler(int signum) {
	/* signal handler for SIGCHLD */
	waitpid(WAIT_ANY, NULL, WNOHANG);
}
static void readSocket(int sockfd, char * buff, size_t bufflen) {
	/* reads GET from socket */
	if (recv(sockfd, buff, bufflen, 0) == -1)
		err(1, "receive failed");	

	// NOTE -> Might not buffer everything. Probably should fix that.
}
int main(int argc, char * argv[])
{
	struct sockaddr_in sockname, client;
	struct sigaction sa;
	char *ep, *inbuff, *tmp;
	char outbuff[256], fName[256];

	int clientlen, sd;

	u_short port;
	u_long p;
	pid_t pid;

	/* check params */
	if (argc != 4)
		usage();

	errno = 0;
	p = strtoul(argv[1], &ep, 10);
	if (&argv[1] == '\0' || *ep != '\0'){
		/* parameter wasn't a number, or was empty */
		fprintf(stderr, "%s - not a number\n", argv[1]);
		usage();
	}
	if ((errno == ERANGE && p == ULONG_MAX) || (p > USHRT_MAX)) {
		/* It's a number, but it either can't fit in an unsigned
		 * long, or it is too big for an unsigned short */
	}
	
	/* Ensure argv[2] is a valid directory */
	
	if (chdir(argv[2]) == -1)
		err(1, "chdir failed");
		
	/* TODO : Ensure argv[3] is a valid file */
	LOG_FILE = argv[3];

	/* now safe to do this */
	port = p;

	memset(&sockname, 0, sizeof(sockname));
	sockname.sin_family = AF_INET;
	sockname.sin_port = htons(port);
	sockname.sin_addr.s_addr = htonl(INADDR_ANY);
	sd = socket(AF_INET, SOCK_STREAM, 0);

	if (sd == -1)
		err(1, "socket failed");
	if (bind(sd, (struct sockaddr *) &sockname, sizeof(sockname)) == -1)
		err(1, "bind failed");
	if (listen(sd, 3) == -1)
		err(1, "listen failed");

	/* We are now bound and listening for connections on "sd" */

	/* Catch SIGCHLD to stop zombie children wandering around */
	sa.sa_handler = kidhandler;
	sigemptyset(&sa.sa_mask);

	/* allow system calls (accept) to restart if interrupted by SIGCHLD */
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1)
		err(1, "sigaction failed");

	/* main loop. Accept connections and do stuff */
	printf("Server up and listening on port %u\n", port);
	for(;;) {
		int clientsd;
		ssize_t written, w;
	
		clientlen = sizeof(&client);
		clientsd = accept(sd, (struct sockaddr *)&client, &clientlen);
		if (clientsd == -1)
			err(1, "accept failed");
		
		/* fork child to deal with each connection */
		pid = fork();
		if (pid == -1)
			internalError((struct sockaddr *)&client, "fork failed", NULL);		

		if (pid == 0) {
			char * getLine;
			int written;
			long lSize;
			FILE * fp;
			
			/* Parse GET */
			//outbuff = malloc(128*sizeof(char));
			tmp = malloc(128*sizeof(char));
			if (tmp == NULL)
				internalError(&client, "malloc failed", NULL);
			inbuff = tmp;
			readSocket(clientsd, inbuff, 128);
			
			getLine = checkGET(inbuff, fName);

			if (getLine == NULL) {
				/* BAD REQUEST */
				sendBadRequestError(clientsd);
			} else {
				/* GET is good. Try reading file & sending */
				fp = fopen(fName, "r");
				if (fp == NULL) {
					if (errno == ENOENT) {
						sendNotFoundError(clientsd);
						logNotFound(getIPString(&client), getLine);
					} else if (errno == EACCES) {
						sendForbiddenError(clientsd);
						logForbidden(getIPString(&client), getLine);
					} else 
						internalError(&client, "fopen failed", getLine);
				} else {
					/* get file size */
					fseek(fp, sizeof(char), SEEK_END);
					lSize = ftell(fp);
					rewind(fp);
					/* send OK and file */
					sendOK(clientsd, lSize);
					written = sendFile(fp, clientsd);
					logOK(getIPString(&client), getLine, written, lSize-1);
				}		
			}

			/* Clean up */
			free(getLine);
			//free(outbuff);
			getLine = NULL;
			inbuff = NULL;
			//outbuff = NULL;

			exit(0);
		}
		close(clientsd);
	}
}

