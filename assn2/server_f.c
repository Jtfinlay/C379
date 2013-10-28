/*
 
Author: James Finlay
Date: Oct. 27th 2013
 
Description:
Performs logging based upon the servers mode of operation
 
License is a MS-PL license
 
This license governs use of the accompanying software. If you use the software,
you accept this license. If you do not accept the license, do not use the 
software.
 
1. Definitions
The terms "reproduce," "reproduction," "derivative works," and "distribution" 
have the same meaning here as under U.S. copyright law.
A "contribution" is the original software, or any additions or changes to the 
software.
A "contributor" is any person that distributes its contribution under this 
license.
"Licensed patents" are a contributor's patent claims that read directly on its 
contribution.
 
2. Grant of Rights
(A) Copyright Grant- Subject to the terms of this license, including the license
conditions and limitations in section 3, each contributor grants you a non-
exclusive, worldwide, royalty-free copyright license to reproduce its 
contribution, prepare derivative works of its contribution, and distribute its 
contribution or any derivative works that you create.
(B) Patent Grant- Subject to the terms of this license, including the license 
conditions and limitations in section 3, each contributor grants you a non-
exclusive, worldwide, royalty-free license under its licensed patents to make, 
have made, use, sell, offer for sale, import, and/or otherwise dispose of its 
contribution in the software or derivative works of the contribution in the 
software.
 
3. Conditions and Limitations
(A) No Trademark License- This license does not grant you rights to use any 
contributors' name, logo, or trademarks.
(B) If you bring a patent claim against any contributor over patents that you 
claim are infringed by the software, your patent license from such contributor 
to the software ends automatically.
(C) If you distribute any portion of the software, you must retain all 
copyright, patent, trademark, and attribution notices that are present in the 
software.
(D) If you distribute any portion of the software in source code form, you may 
do so only under this license by including a complete copy of this license with 
your distribution. If you distribute any portion of the software in compiled or 
object code form, you may only do so under a license that complies with this 
license.
(E) The software is licensed "as-is." You bear the risk of using it. The 
contributors give no express warranties, guarantees or conditions. You may have 
additional consumer rights under your local laws which this license cannot 
change. To the extent permitted under your local laws, the contributors exclude 
the implied warranties of merchantability, fitness for a particular purpose and 
non-infringement.
 
*/

/*
 * Can be compiled using 'make server_f' or 'make all'.
 * Created for ohaton.cs.ualberta.ca
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

#include "helpers.h"

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
}
int main(int argc, char * argv[])
{
	struct sockaddr_in sockname, client;
	struct sigaction sa;
	char *ep, *inbuff, *tmp;
	char outbuff[256];

	int clientlen, sd;

	u_short port;
	u_long p;
	pid_t pid;
	
		/* daemonize */
#ifndef DEBUG
	/* don't daemonize if we compile with -DDEBUG */
	if (daemon(1,1) == -1)
		err(1, "daemon() failed");
#endif

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
		fprintf(stderr, "%s - value out of range\n", argv[1]);
		usage();
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
			internalError((struct sockaddr *)&client, 
				"fork failed", NULL);		

		if (pid == 0) {
			char * getLine, *fName;
			int valid, written;
			long lSize;
			FILE * fp;
			
			/* Parse GET */
			inbuff = malloc(12800*sizeof(char));
			fName = malloc(256*sizeof(char));
			if (inbuff == NULL || fName == NULL)
				internalError(&client, "malloc failed", NULL);

			readSocket(clientsd, inbuff, 12800);

			getLine = malloc(256*sizeof(char));
			if (getLine == NULL)
				internalError(&client, "malloc failed", NULL);
			
			valid = checkGET(inbuff, fName, getLine);
			fName++;


			if (valid == 0) {
				/* BAD REQUEST */
				sendBadRequestError(clientsd);
				logBadRequest(getIPString(&client), getLine);
			} else {
				/* GET is good. Try reading file & sending */

				fp = fopen(fName, "r");
				if (fp == NULL) {
					if (errno == ENOENT) {
						sendNotFoundError(clientsd);
						logNotFound(
							getIPString(&client),
							 getLine);
					} else if (errno == EACCES) {
						sendForbiddenError(clientsd);
						logForbidden(
							getIPString(&client), 
							getLine);
					} else 
						internalError(&client, 
							"fopen failed", 
							getLine);
				} else {
					
					/* get file size */
					fseek(fp, sizeof(char), SEEK_END);
					lSize = ftell(fp);
					rewind(fp);
					/* send OK and file */
				
					sendOK(clientsd, lSize);
					written = sendFile(fp, clientsd);
					logOK(getIPString(&client), getLine, 
						written, lSize-1);
				}		
				fclose(fp);
			}

			/* Clean up */
			fName--;
			free(getLine);
			free(inbuff);
			free(fName);
			fName = NULL;
			getLine = NULL;
			inbuff = NULL;

			exit(0);
		}
		close(clientsd);
	}
}

