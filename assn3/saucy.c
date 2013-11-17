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

pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
	pthread_t rockets[MAX_ROCKETS];
	int c;
	int i;

	/* check inputs */

	/* setup */
	setup();

	/* create threads */

	/* loop to process input */
	while (1) {
		c = getch();
		if (c == 'Q') break;
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
	mvprintw(LINES-1, 0, "'Q' to quit ',' moves left '.' moves right SPACE first : Escaped 0\n");
}







