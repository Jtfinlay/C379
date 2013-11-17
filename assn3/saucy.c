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

pthread_mutex_t mxCurses = PTHREAD_MUTEX_INITIALIZER;

void *animateRocket();
void drawUser(int x);

int main(int argc, char *argv[])
{
	pthread_t rockets[MAX_ROCKETS];	/* rocket array! */
	int c;				/* user input */
	int x;				/* user position */	

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
		if (c == ' ') {

		}

		/* DRAW PLAYER */
		drawUser(x);
	}
	

	/* clean up */
	endwin();
	return 0;
}

int setup()
{

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
void *animateRocket() {

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








