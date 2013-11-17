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

#define MAX_ROCKETS	25   	/* Maximum rockets available */
#define MAX_SAUCERS	10	/* Maximum saucers available */
#define TIME_DELAY    	20000	/* timeunits in microseconds */
#define LAUNCH_DELAY	100000	/* time between potential saucer launches */
#define ROCKET_V	5	/* speed of rocket */
#define MAX_SAUCE_V	7	/* max speed of saucers */
#define SAUCER_ROWS   	5	/* number of rows for saucers */

typedef struct entity {
	int x;
	int y;
	int alive;
} Rocket, Saucer;


pthread_mutex_t mxCurses =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxRockets = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxSaucers = PTHREAD_MUTEX_INITIALIZER;

Rocket rockets[MAX_ROCKETS];	/* rocket data array */
pthread_t tRockets[MAX_ROCKETS];/* rocket thread array */

void setup();
void launchRocket(Rocket r[], pthread_t th[], int x);
void *animateRocket(void* r);
void *saucerManager();
void drawUser(int x);

int main(int argc, char *argv[])
{
	int c;				/* user input */
	int x;				/* user position */
	int i;	

	/* check input params */

	/* setup */
	setup();
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
		if (c == ' ') launchRocket(rockets, tRockets, x);
		
		/* DRAW PLAYER */
		drawUser(x);
	}
	

	/* clean up */
	endwin();
	return 0;
}
/* set up game */
void setup()
{
	int i=0;

	/* set up curses */
	initscr();
	crmode();
	noecho();
	clear();
	
	/* draw start info */
	mvprintw(LINES-1, 0, " 'Q' to quit ',' moves left '.' moves right SPACE first : Escaped 0\n");
	drawUser(COLS/2);
}
/* try to launch rocket */
void launchRocket(Rocket r[], pthread_t th[], int x) {
	int i;

	pthread_mutex_lock(&mxRockets);
	    for (i=0; i<MAX_ROCKETS; i++) {
		if (!r[i].alive) {
			r[i].x = x;
			r[i].y = LINES-1;
			if (pthread_create(&th[i], NULL, animateRocket, &r[i])) {
				fprintf(stderr, "error creating thread");
				endwin();
				exit(0);
			}
			break;
		}
	    }
	pthread_mutex_unlock(&mxRockets);
}
/* manages rocket propulsion */
void *animateRocket(void *rocket) {
	int x, y;
	Rocket *r = rocket;

	r->alive = 1;
	r->y = LINES-3;

	while (1) 
	{
		usleep(5*TIME_DELAY);

		/* clear rocket */
		pthread_mutex_lock(&mxCurses);
		    move(y,x);
		    addch(' ');
		pthread_mutex_unlock(&mxCurses);

		/* update & get position */
		pthread_mutex_lock(&mxRockets);
		    /* move up */
		    r->y--;

		    /* check boundary */
		    if (r->y < 0) {
			r->alive = 0;
			pthread_mutex_unlock(&mxRockets);
			return;
	    	    }
	
		    /* set tmp variables */
		    x = r->x;
		    y = r->y;
		pthread_mutex_unlock(&mxRockets);
		
		/* draw rocket */
		pthread_mutex_lock(&mxCurses);
		    move(y,x);
		    addch('^');
		    refresh();
		pthread_mutex_unlock(&mxCurses);
		
	}

}
void *saucerManager() {
	Saucer saucers[MAX_SAUCERS];	/* saucer data array */
	pthread_t tSaucers[MAX_SAUCERS];/* saucer thread array */

	while (1) {
		
		/* Randomly launch saucers */

		

		/* Check rocket/saucer collisions */


	}

}
/* draw player */
void drawUser(int x) {
	pthread_mutex_lock(&mxCurses);
	    move(LINES-2, x-1);
	    addch(' ');
	    move(LINES-2, x);
	    addch('|');
	    move(LINES-2, x+1);
	    addch(' ');
   	    refresh();
	pthread_mutex_unlock(&mxCurses);
}








