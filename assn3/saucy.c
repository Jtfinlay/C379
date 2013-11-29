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

#define ROCKETS_ON_HIT	5	/* How many rockets gained on hitting alien */
#define MAX_ESCAPE	20	/* Maximum escaped until game over */
#define START_ROCKETS	100	/* Maximum available at start */
#define MAX_ROCKETS_T	25   	/* Maximum rockets available */
#define MAX_SAUCERS	10	/* Maximum saucers available */
#define TIME_DELAY    	20000	/* timeunits in microseconds */
#define LAUNCH_DELAY	200000	/* time between potential saucer launches */
#define ROCKET_V	5	/* speed of rocket */
#define MAX_SAUCE_V	7	/* max speed of saucers */
#define SAUCER_ROWS   	5	/* number of rows for saucers */
#define INSTRUCT	"'Q' to quit ',' moves left '.' moves right SPACE first"

typedef struct entity {
	int x;
	int y;
	float veloc;
	int alive;
} Rocket, Saucer;


pthread_mutex_t mxCurses =  PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxRockets = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mxSaucers = PTHREAD_MUTEX_INITIALIZER;

Rocket rockets[MAX_ROCKETS_T];	/* rocket data array */
pthread_t tRockets[MAX_ROCKETS_T];/* rocket thread array */
Saucer saucers[MAX_SAUCERS];	/* saucer data array */
pthread_t tSaucers[MAX_SAUCERS];/* saucer thread array */

int ESCAPED = 0;
int HITS = 0;
int ROCKETS = 0;

int game_over=0;

static void usage();
void setup();
void drawUser(int x);
void drawScore();
void *scoreManager();
void *inputManager();
void *saucerManager();

void launchRocket(Rocket r[], pthread_t th[], int x);
void *animateRocket(void* r);

void launchSaucer(Saucer s[], pthread_t th[], int y);
void *animateSaucer(void *saucer);

int checkForRocket(int x, int y);
int checkForSaucer(int x, int y);
void killSaucerAt(int x, int y);
void killRocketAt(int x, int y);
int countRocketsLiving();

static void usage()
{
	extern char * __progname;
	fprintf(stderr, "usage: %s\n", __progname);
	exit(1);
}
int main(int argc, char *argv[])
{
	pthread_t sauceBoss;	/* saucer manager */
	pthread_t scoreBoss;	/* score manager */
	pthread_t inputBoss;	/* input manager */
	int i;	

	/* check input params */
	if (argc != 1)
		usage();

	/* setup */
	setup();

	/* create threads */
	if (pthread_create(&sauceBoss, NULL, saucerManager, NULL) ||
	    pthread_create(&scoreBoss, NULL, scoreManager, NULL) ||
	    pthread_create(&inputBoss, NULL, inputManager, NULL)) {

		fprintf(stderr, "error creating thread");
		endwin();
		exit(0);
	}

	while( !game_over );
	

	/* clean up */
	pthread_cancel(sauceBoss);
	pthread_cancel(scoreBoss);
	pthread_cancel(inputBoss);
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
	curs_set(0);
	clear();
	
	/* draw start info */
	ROCKETS = START_ROCKETS;
	drawScore();
	drawUser(COLS/2);
}
void drawScore() {
	pthread_mutex_lock(&mxCurses);
		move(LINES-2, 0);
		clrtoeol();
		mvprintw(LINES-1, 0, "%s", INSTRUCT);
		mvprintw(LINES-2, 0, " Escaped %d, Hits %d, Rockets %d", 
			ESCAPED, HITS, ROCKETS);
	pthread_mutex_unlock(&mxCurses);
}
void *scoreManager() {
	while (1) {

		usleep(TIME_DELAY);

		drawScore();

		/* All rockets gone? */
		if (ROCKETS <= 0 && countRocketsLiving() == 0) {
			printf("The fuck.\n");
			printf("Game over! You killed %d aliens before running %s\n",
				HITS, "out of ammo.");
			game_over++;
		}

		/* All aliens escaped? */
		if (ESCAPED >= MAX_ESCAPE) {
			printf("Game over! You killed %d aliens they overwhelmed %s\n",
				HITS, "you.\n");
			game_over++;
		}
	}
}
void *inputManager() {
	int c;			/* user input */
	int x;			/* user position */

	x = COLS/2;

	while (1) {
		c = getch();

		/* QUIT */
		if (c == 'Q') game_over++;
	
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
}
/* draw player */
void drawUser(int x) {
	pthread_mutex_lock(&mxCurses);
	    move(LINES-3, x-1);
	    addch(' ');
	    move(LINES-3, x);
	    addch('|');
	    move(LINES-3, x+1);
	    addch(' ');
   	    refresh();
	pthread_mutex_unlock(&mxCurses);
}
/* try to launch rocket */
void launchRocket(Rocket r[], pthread_t th[], int x) {
	int i;

	/* out of rockets? */
	if (ROCKETS <= 0)
		return;

	/* make new rocket */
	pthread_mutex_lock(&mxRockets);
	    for (i=0; i<MAX_ROCKETS_T; i++) {
		if (!r[i].alive) {
			ROCKETS--;
			r[i].alive = 1;
			r[i].x = x;
			r[i].y = LINES-4;
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

	while (1) 
	{
		usleep(4*TIME_DELAY);

		/* clear rocket */
		pthread_mutex_lock(&mxCurses);
		    move(y,x);
		    addch(' ');
		    refresh();
		pthread_mutex_unlock(&mxCurses);

		
		/* ensure not dead */
		if (r->alive==0)
			pthread_exit(NULL);

		/* check for collisions */
		if (checkForSaucer(x+1,y)) {
			r->alive = 0;
			killSaucerAt(x+1,y);
			pthread_exit(NULL);
		}

		/* update & get position */
		pthread_mutex_lock(&mxRockets);
		    /* move up */
		    r->y--;

		    /* check boundary */
		    if (r->y < 0) {
			r->alive = 0;
			pthread_mutex_unlock(&mxRockets);
			pthread_exit(NULL);
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
	int row;

	while (1) 
	{
		usleep(LAUNCH_DELAY);

		/* Randomly launch saucers */
		if (rand()%100 > 10) continue;
		row = rand()%SAUCER_ROWS;

		launchSaucer(saucers, tSaucers, row);
	}

}
void launchSaucer(Saucer s[], pthread_t th[], int y) {
	int i;

	pthread_mutex_lock(&mxSaucers);
	    for (i=0; i<MAX_SAUCERS; i++) {
		if (!s[i].alive) {
			s[i].alive = 1;
			s[i].x = 0;
			s[i].y = y;
			s[i].veloc = (double)((rand()%20))/10.0+1.0;
			if (pthread_create(&th[i], NULL, animateSaucer, &s[i])) {
				fprintf(stderr, "error creating thread");
				endwin();
				exit(0);
			}
			break;
		}
	    }
	pthread_mutex_unlock(&mxSaucers);
}
void *animateSaucer(void *saucer) {
	int x, y, i;
	Saucer *s = saucer;

	while (1) 
	{
		usleep(5*TIME_DELAY);

		/* clear saucer */
		pthread_mutex_lock(&mxCurses);
		    move(y,x);
		    addstr("     ");
		    refresh();
		pthread_mutex_unlock(&mxCurses);

		
		/* ensure not dead */
		if (s->alive==0)
			pthread_exit(NULL);
		
		/* check for collisions */
		for (i=0; i<=(s->veloc); i++) {
			if (checkForRocket(i+x,y)) {
				s->alive = 0;
				HITS++;
				ROCKETS += ROCKETS_ON_HIT;
				killRocketAt(i+x,y);
				pthread_exit(NULL);
			}
		}


		/* update & get position */
		pthread_mutex_lock(&mxSaucers);
		    /* move right */
		    s->x += s->veloc;

		    /* check boundary */
		    if (s->x > COLS-6) {
			pthread_mutex_unlock(&mxSaucers);
			s->alive = 0;
			ESCAPED++;
			pthread_exit(NULL);
		    }
	
		    /* set tmp variables */
		    x = s->x;
		    y = s->y;
		pthread_mutex_unlock(&mxSaucers);

		/* draw rocket */
		pthread_mutex_lock(&mxCurses);
		    move(y,x);
		    addstr("<--->");
		    refresh();
		pthread_mutex_unlock(&mxCurses);
		
	}
}
int checkForRocket(int x, int y) {
	int result = FALSE;
	pthread_mutex_lock(&mxCurses);
	    move(y,x);
	    if (inch() == '^')
		result = TRUE;
	pthread_mutex_unlock(&mxCurses);
	return result;
}
int checkForSaucer(int x, int y) {
	int result = FALSE;
	pthread_mutex_lock(&mxCurses);
	    move(y,x);
	    if (inch() == '<') result = TRUE;
	    else if (inch() == '-') result = TRUE;
	    else if (inch() == '>') result = TRUE;		
	pthread_mutex_unlock(&mxCurses);
	return result;
}
void killSaucerAt(int x, int y) {
	int i, n;
	pthread_mutex_lock(&mxSaucers);
	    for (i=0; i<MAX_SAUCERS; i++) {
		for (n=x-5; n<=x+5; n++) {
			if (saucers[i].x==n && saucers[i].y==y) {
				if(saucers[i].alive==1) {
					HITS++;
					ROCKETS += ROCKETS_ON_HIT;
				}
				saucers[i].alive = 0;
			}
		}
	    }
	pthread_mutex_unlock(&mxSaucers);
}
void killRocketAt(int x, int y) {
	int i;
	pthread_mutex_lock(&mxRockets);
	    for (i=0; i<MAX_ROCKETS_T; i++) {
		if (rockets[i].x==x && rockets[i].y==y) {
			rockets[i].alive = 0;
		}
	    }
	pthread_mutex_unlock(&mxRockets);
}
int countRocketsLiving() {
	int i, c;

	pthread_mutex_lock(&mxRockets);
		c=0;
		for (i=0; i<MAX_ROCKETS_T; i++) {
			if (rockets[i].alive == 1)
				c++;
		}
	pthread_mutex_unlock(&mxRockets);
	return c;
}



