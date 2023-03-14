// Allison Tipan V00848862
/*
 * kosmos-mcv.c (mutexes & condition variables)
 *
 * For UVic CSC 360, Summer 2022
 *
 * Here is some code from which to start.
 *
 * PLEASE FOLLOW THE INSTRUCTIONS REGARDING WHERE YOU ARE PERMITTED
 * TO ADD OR CHANGE THIS CODE. Read from line 133 onwards for
 * this information.
 */

#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "logging.h"


/* Random # below threshold indicates H; otherwise C. */
#define ATOM_THRESHOLD 0.55
#define DEFAULT_NUM_ATOMS 40

#define MAX_ATOM_NAME_LEN 10
#define MAX_KOSMOS_SECONDS 5

/* Global / shared variables */
int  cNum = 0, hNum = 0;
long numAtoms;


/* Function prototypes */
void kosmos_init(void);
void *c_ready(void *);
void *h_ready(void *);
void make_radical(int, int, int, char *);
void wait_to_terminate(int);


/* Needed to pass legit copy of an integer argument to a pthread */
int *dupInt( int i )
{
	int *pi = (int *)malloc(sizeof(int));
	assert( pi != NULL);
	*pi = i;
	return pi;
}


int main(int argc, char *argv[])
{
	long seed;
	numAtoms = DEFAULT_NUM_ATOMS;
	pthread_t **atom;
	int i;
	int status;

	if ( argc < 2 ) {
		fprintf(stderr, "usage: %s <seed> [<num atoms>]\n", argv[0]);
		exit(1);
	}

	if ( argc >= 2) {                                        // convert args to ints
		seed = atoi(argv[1]);
	}

	if (argc == 3) {                                         // 
		numAtoms = atoi(argv[2]);
		if (numAtoms < 0) {
			fprintf(stderr, "%ld is not a valid number of atoms\n",
				numAtoms);
			exit(1);
		}
	}

    kosmos_log_init();
	kosmos_init();

	srand(seed);
	atom = (pthread_t **)malloc(numAtoms * sizeof(pthread_t *));
	assert (atom != NULL);
	for (i = 0; i < numAtoms; i++) {
		atom[i] = (pthread_t *)malloc(sizeof(pthread_t));
		if ( (double)rand()/(double)RAND_MAX < ATOM_THRESHOLD ) {
			hNum++;
			status = pthread_create (
					atom[i], NULL, h_ready,
					(void *)dupInt(hNum)
				);
		} else {
			cNum++;
			status = pthread_create (
					atom[i], NULL, c_ready,
					(void *)dupInt(cNum)
				);
		}
		if (status != 0) {
			fprintf(stderr, "Error creating atom thread\n");
			exit(1);
		}
	}

    /* Determining the maximum number of ethynyl radicals is fairly
     * straightforward -- it will be the minimum of the number of
     * hNum and cNum/2.
     */

    int max_radicals = (hNum < cNum/2 ? hNum : (int)(cNum/2));
#ifdef VERBOSE
    printf("Maximum # of radicals expected: %d\n", max_radicals);
#endif

    wait_to_terminate(max_radicals);
}


/*
* Now the tricky bit begins....  All the atoms are allowed
* to go their own way, but how does the Kosmos ethynyl-radical
* problem terminate? There is a non-zero probability that
* some atoms will not become part of a radical; that is,
* many atoms may be blocked on some condition variable of
* our own devising. How do we ensure the program ends when
* (a) all possible radicals have been created and (b) all
* remaining atoms are blocked (i.e., not on the ready queue)?
*/


/*
 * ^^^^^^^
 * DO NOT MODIFY CODE ABOVE THIS POINT.
 *
 *************************************
 *************************************
 *
 * ALL STUDENT WORK MUST APPEAR BELOW.
 * vvvvvvvv
 */


/* 
 * DECLARE / DEFINE NEEDED VARIABLES IMMEDIATELY BELOW.
 */

pthread_mutex_t talking_stick;

// calling them queues are not literal, I'm just using the conditional variables like a line up
pthread_cond_t carbon_queue;
pthread_cond_t hydrogen_queue;

int c_count;
int h_count;
int radicals;

// buffer to hold ids, index [0]=c1, [1]=c2, [2]=h
int atom_names[3]; 
/*
 * FUNCTIONS YOU MAY/MUST MODIFY.
 */

void kosmos_init() {
    
}

// there's lines of code that are taken from the appendices but mostly just concept/general ideas.
void *h_ready( void *arg ) {
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN];

    sprintf(name, "h%03d", id);
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif

    // my turn 
    pthread_mutex_lock(&talking_stick);
    
    // if there's already a hydrogen lined up, I'll just wait here
    while (h_count >= 1){
        pthread_cond_wait(&hydrogen_queue, &talking_stick);
    }
    
    // add my name
    atom_names[2] = id;
    h_count++;
    
    // if the stars align, make the radical and reset hydrogen/carbon counters
    if ((h_count==1) && (c_count==2)){
        make_radical(atom_names[0], atom_names[1], atom_names[2], name);
        radicals++;
        h_count--;
        c_count = c_count-2;
    }
    
    // give someone else a turn. Wake up the carbons  (what do we even pay them for?; we don't pay them, sir)
    pthread_mutex_unlock(&talking_stick);
    pthread_cond_signal(&carbon_queue);
    

	return NULL;
}


void *c_ready( void *arg ) {
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN];

    sprintf(name, "c%03d", id);
    
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif

    // my turn 
    pthread_mutex_lock(&talking_stick);
    
    // if there's already a couple carbons lined up, I'll just wait here
    while (c_count >= 2){
        pthread_cond_wait(&carbon_queue, &talking_stick);
    }
    
    // add my name to list, depending on if I'm the first or second carbon
    if (c_count == 1){
        atom_names[1] = id;
    } else {
        atom_names[0] = id;
    }
    c_count++;
    
    // if the stars align, make the radical and reset hydrogen/carbon counters
    if ((h_count==1) && (c_count==2)){
        make_radical(atom_names[0], atom_names[1], atom_names[2], name);
        radicals++;
        h_count--;
        c_count = c_count-2;
    }
    
    // give someone else a turn. Wake up the hydrogens
    pthread_mutex_unlock(&talking_stick);
    pthread_cond_signal(&hydrogen_queue);
    
    
	return NULL;
}

    
void make_radical(int c1, int c2, int h, char *maker)
{
#ifdef VERBOSE
	fprintf(stdout, "A ethynyl radical was made: c%03d  c%03d  h%03d\n",
		c1, c2, h);
#endif
    kosmos_log_add_entry(radicals, c1, c2, h, maker);
}

void wait_to_terminate(int expected_num_radicals) {
    /* A rather lazy way of doing it, but good enough for this assignment. */
    sleep(MAX_KOSMOS_SECONDS);
    kosmos_log_dump();
    exit(0);
}
