/*
 * Copyright
 */
 
/* 
 * How to compile & run
 */

#include <sys/param.h>

#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <netinet/in.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shelpers.h"

struct reply {
	char *buf; /* buffer to store the characters in */
	char *bp; /* where we are in buffer */
	size_t bs; /*total size of buffer */
};
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
	struct reply re; /* reply string */
};

#define MAXCONN 256 /* Max # of connections possible */
struct con connections[MAXCONN];

#define BUF_ASIZE 256 /* how much buf to allocate at once */

/* states used in struct con */
#define STATE_UNUSED 0
#define STATE_READING 1
#define STATE_WRITING 2

char * LOG_FILE;

static void usage()
{
	extern char * __progname;
	fprintf(stderr, "usage: %s portnumber dir logfile\n", __progname);
	exit(1);
}

/* get free connection structure, to save a new connection in */
struct con * get_free_conn()
{
	int i;
	for (i=0; i<MAXCONN; i++) {
		if (connections[i].state == STATE_UNUSED)
			return(&connections[i]);
	}
	return(NULL); /* we're all full - indicate to caller */
}

/* 
 * close or initialize a connection - reset a connection to default
 * unused state.
 */
void closecon(struct con *cp, int initflag)
{
	if (!initflag) {
		if (cp->sd != -1)
			close(cp->sd); /* close socket */
		free(cp->buf); /* free up our buffer */
	}
	memset(cp, 0, sizeof(struct con)); /* zero out the con struct */
	cp->buf = NULL; 
	cp->sd = -1;
}

/* deal with a connection that we want to write stuff to */
void handlewrite(struct con *cp)
{
	int valid;
	ssize_t i;
	char *buf, *fLine;
	char fName[256];
	
	buf = malloc(BUF_ASIZE*sizeof(char));
	fLine = malloc(BUF_ASIZE*sizeof(char));
	if (buf == NULL || fLine == NULL)
		err(1, "malloc fail");
		
	printf("Here we check the GET of '%s'\n", cp->buf);
	
	valid = checkGET(cp->buf, fName, fLine);
	
	if (valid == 0) { 
		/* BAD REQUEST */
		printf("Bad Request.\n");
	} else {
		FILE * fp;
		
		fp = fopen(fName, "r");
		if (fp == NULL) {
			if (errno = ENOENT) {
				/* NOT FOUND */
				printf("Not Found.\n");
			} else if (errno == EACCES) {
				/* FORBIDDEN */
				printf("Forbidden.\n");
			} else {
				/* INTERNAL ERROR */
				printf("Internal Error.\n");
			}
		} else {
			/* OK! */
			printf("Ok.\n");
		}
	}
	
	i = write(cp->sd, "this is a get\n", 15);
	
	// Clean
	free(fLine);
	free(buf);
	fLine = NULL;
	buf = NULL;
	
	if (i == -1) {
		if (errno != EAGAIN) {
			/* write failed */
			closecon(cp, 0);
		}
		return;
	}
	
//	cp->bp += i; /* move where we are */
//	cp->bl -= i; /* decrease how much we have left to write */
//	if (cp->bl == 0) {
		/* we wrote it all out, so kill client */
	closecon(cp, 0);
	//}
}

void handleread(struct con *cp) {
	ssize_t len;
	
	/* ensure enough room to do a decent sized read */
	if (cp->bl < 10) {
		char *tmp;
		tmp = realloc(cp->buf, (cp->bs + BUF_ASIZE) * sizeof(char));
		if (tmp == NULL) {
			/* no memory */
			closecon(cp, 0);
			return;
		}
		
		cp->buf = tmp;
		cp->bs += BUF_ASIZE;
		cp->bl += BUF_ASIZE;
		cp->bp = cp->buf + (cp->bs - cp->bl);
	}
	
	len = read(cp->sd, cp->bp, cp->bl);
	if (len == 0) {
		printf("Connection closed.\n");
		/* 0 byte read means connection closed */
		closecon(cp, 0);
		return;
	}
	if (len == -1) {
		if (errno != EAGAIN) {
			/* read failed */
			err(1, "read failed! sd %d\n", cp->sd);
			closecon(cp, 0);
		}
		return;
	}
	printf("read: '%s'\n", cp->bp);
	/* have something to read. Change pointer */
	cp->bp += len;
	cp->bl -= len;
	
	/* change state ? */
	if (*(cp->bp -1) == '\n') {
		cp->state = STATE_WRITING;
		cp->bl = cp->bp - cp->buf; /* how much we will write */
		cp->bp = cp->buf; /* and we'll start from here */
	}
}

int main(int argc, char *argv[])
{
	struct sockaddr_in sockname;
	int max = -1, omax; /* the biggest value sd. for select */
	int sd; /* listen socket */
	fd_set *readable = NULL, *writable = NULL; /* fd_sets for select */
	u_short port;
	u_long p;
	char *ep;
	int i;
	FILE * fp;
	
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
	if (&argv[1] == '\0' || *ep != '\0') {
		/* parameter wasn't a number, or was empty */
		fprintf(stderr, "%s - not a number\n", argv[1]);
		usage();
	}
	if (chdir(argv[2]) == -1) {
		fprintf(stderr, "%s - dir does not exist\n", argv[2]);
		usage();
	}
	fp = fopen(argv[3], "w");
	if (fp == NULL) {
		fprintf(stderr, "%s - file could not be created\n", argv[3]);
		usage();
	}
	
	/* now safe to do this */
	LOG_FILE = argv[3];
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

	printf("Server up and listening for connections on port %u\n", port);
	
	/* initailize all our connection structures */
	for (i=0; i < MAXCONN; i++)
		closecon(&connections[i], 1);

	for(;;) {
		int i;
		int maxfd = -1;
		
		/*
		 * initalize the fd_sets to keep track
		 * of readable and writable sockets.
		 */
		
		omax = max;
		max = sd; /* listen socket */
		
		for (i = 0; i < MAXCONN; i++) {
			if (connections[i].sd > max)
				max = connections[i].sd;
		}
		if (max > omax) {
			/* we need bigger fd_sets allocated */
			
			/* free old ones */
			free(readable);
			free(writable);
			
			/* allocate fd_sets for select */
			readable = (fd_set *) calloc(howmany(max + 1, NFDBITS),
				sizeof(fd_mask));
			if (readable == NULL)
				err(1, "out of memory");
			writable = (fd_set *)calloc(howmany(max + 1, NFDBITS),
				sizeof(fd_mask));
			if (writable == NULL)
				err(1, "out of memory");
			omax = max;
		} else {
			/* allocated sets are big enough, so just clear to 0 */
			memset(readable, 0, howmany(max+1, NFDBITS) *
				sizeof(fd_mask));
			memset(writable, 0, howmany(max+1, NFDBITS) *
				sizeof(fd_mask));
		}
		
		/* 
		 * decide which sockets we are interested in reading 
		 * and writing, by setting corresponding bt in readable
		 * and writable fd_sets
		 */
		
		/* always interested in reading from listen socket */
		FD_SET(sd, readable);
		if(maxfd < sd)
			maxfd = sd;
			
		for (i = 0; i<MAXCONN; i++) {
			if (connections[i].state == STATE_READING) {
				FD_SET(connections[i].sd, readable);
				if (maxfd < connections[i].sd)
					maxfd = connections[i].sd;
			}
			if (connections[i].state == STATE_WRITING) {
				FD_SET(connections[i].sd, writable);
				if (maxfd < connections[i].sd)
					maxfd = connections[i].sd;
			}
		}
		
		/* we can now call select. Finally.. */
		i = select(maxfd + 1, readable, writable, NULL, NULL);
		if (i == -1 && errno != EINTR)
			err(1, "select failed");
		if (i > 0) {
			
				/* check listen socket. If readable, new conenction */
				if (FD_ISSET(sd, readable)) {
					struct con *cp;
					int newsd;
					socklen_t slen;
					struct sockaddr_in sa;
					
					slen = sizeof(sa);
					newsd = accept(sd, (struct sockaddr *) &sa,
						&slen);
					if (newsd == -1)
						err(1, "accept failed");
					
					cp = get_free_conn();
					if (cp == NULL) {
						/*
						 * no connection structures so
						 * drop client
						 */
						close(newsd);
					} else {
						/* 
						 * have new connection! set him
						 * to READING so we can do stuff
						 */
						cp->state = STATE_READING;
						cp->sd = newsd;
						cp->slen = slen;
						memcpy(&cp->sa, &sa, sizeof(sa));
					}
				}
				
				/*
				 * iterate through all connections, check
				 * to see if readable or writable. Do read
				 * or write accordingly
				 */
				
				for (i =0; i<MAXCONN; i++) {
					if ((connections[i].state == STATE_READING) &&
						FD_ISSET(connections[i].sd, readable))
						handleread(&connections[i]);
					if ((connections[i].state == STATE_WRITING) &&
						FD_ISSET(connections[i].sd, writable))
						handlewrite(&connections[i]);
				}
			}
	}
}
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		
		





