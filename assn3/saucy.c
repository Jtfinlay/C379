/*
 * Copyright stuff here.
 */

/* 
 * About program
 * 
 * How to launch
 */

#include <stdio.h>
#include <curses.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define MAX_ROCKETS   10   	/* Maximum rockets available */
#define TIME_DELAY    20000	/* timeunits in microseconds */
#define SAUCER_ROWS   5		/* number of rows for saucers */

typedef struct Rocket {
	int x;
	int y;
	int alive;
} Rocket;

pthread_mutex_t mxCurses =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxRockets = PTHREAD_MUTEX_INITIALIZER;

void setup(Rocket rockets[]);
void *animateRocket(void* r);
void drawUser(int x);

int main(int argc, char *argv[])
{
	Rocket rockets[MAX_ROCKETS];	/* rocket data array */
	pthread_t tRockets[MAX_ROCKETS];/* rocket thread array */
	int c;				/* user input */
	int x;				/* user position */
	int i;	

	/* check input params */

	/* setup */
	setup(rockets);
	x = COLS/2;

	/* create threads */

	/* loop to process input */
	while (1) {
		c = getch();

		/* QUIT */
		if (c == 'Q') break;
	
		/* MOVE PLAYER */
		if (c == ',') 	x--;
		if (c == '.') 	x++;
		if (x<0) 	x=0;
		if (x>COLS-1)	x=COLS-1;

		/* FIRE ROCKET */
		if (c == ' ') {
			pthread_mutex_lock(&mxRockets);
			for (i=0; i<MAX_ROCKETS; i++) {
				if (!rockets[i].alive) {
	if (pthread_create(&tRockets[i], NULL, animateRocket, &rockets[i])) {
		fprintf(stderr, "error creating thread");
		endwin();
		exit(0);
	}
				break;
				}
			}
			pthread_mutex_unlock(&mxRockets);
		}

		/* DRAW PLAYER */
		drawUser(x);
	}
	

	/* clean up */
	endwin();
	return 0;
}

void setup(Rocket rockets[])
{
	int i=0;

	/* set up rockets */
	for (i=0; i<MAX_ROCKETS; i++) {
		rockets[i].x = 5;
		rockets[i].y = LINES-3;
		rockets[i].alive = 0;
	}

	/* set up curses */
	initscr();
	crmode();
	noecho();
	clear();
	
	/* draw start info */
	mvprintw(LINES-1, 0, " 'Q' to quit ',' moves left '.' moves right SPACE first : Escaped 0\n");
	drawUser(COLS/2);
}

/* manages rocket propulsion */
void *animateRocket(void *r) {
	Rocket *rocket = r;
}
/* draw player */
void drawUser(int x) {
	move(LINES-2, x-1);
	addch(' ');
	move(LINES-2, x);
	addch('|');
	move(LINES-2, x+1);
	addch(' ');
	refresh();
}








