// Allison Tipan V00848862
/*
 * kosmos-sem.c (semaphores)
 *
 * For UVic CSC 360, Summer 2022
 *
 * Here is some code from which to start.
 *
 * PLEASE FOLLOW THE INSTRUCTIONS REGARDING WHERE YOU ARE PERMITTED
 * TO ADD OR CHANGE THIS CODE. Read from line 136 onwards for
 * this information.
 */

#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
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

	if ( argc >= 2) {
		seed = atoi(argv[1]);
	}

	if (argc == 3) {
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
* many atoms may be blocked on some semaphore of our own
* devising. How do we ensure the program ends when
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

int radicals;
int num_free_c;
int num_free_h;

int combining_c1;
int combining_c2;
int combining_h;

sem_t mutex;
sem_t wait_c;
sem_t wait_h;


/*
 * FUNCTIONS YOU MAY/MUST MODIFY.
 */

void kosmos_init() {
    // initialize needed semaphores
    sem_init(&wait_c, 0, 2);             // only need 2 carbons
    sem_init(&wait_h, 0, 1);             // only need 1 hydrogen
    sem_init(&mutex, 0, 1);              // sem-mutex to make sure only one atom in critical section
}

// there's lines of code that are taken from the appendices but mostly just concept/general ideas.
void *h_ready( void *arg )
{
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN];

    sprintf(name, "h%03d", id);
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif

    // if there is a hydrogen spot open, I continue, otherwise blocked.
    sem_wait(&wait_h);
    
    // using a binary semaphore as a mutex, to make sure no one else is modifying shared variables
    sem_wait(&mutex);
    
    // get the value of the number of carbons/hyrdogens available from their semaphores
    // add my name to combining_h
    sem_getvalue(&wait_c, &num_free_c);
    sem_getvalue(&wait_h, &num_free_h);
    combining_h = id;
    
    // if there are 2 carbons available, make the radical, increment radical count and post all semaphores used
    if ( (num_free_c == 0) && (num_free_h == 0) ) {
        make_radical(combining_c1, combining_c2, combining_h, name);
        radicals++;
        sem_post(&wait_h);
        sem_post(&wait_c);
        sem_post(&wait_c);
    }
    
    // give someone else a turn
    sem_post(&mutex);


	return NULL;
}


void *c_ready( void *arg )
{
	int id = *((int *)arg);
    char name[MAX_ATOM_NAME_LEN];

    sprintf(name, "c%03d", id);
    
#ifdef VERBOSE
	printf("%s now exists\n", name);
#endif    

    
    // if there is a carbon spot open, I continue, otherwise blocked.   
    sem_wait(&wait_c);
    
    // my turn
    sem_wait(&mutex);
    
    // get the carbon and hydrogen sem counts
    sem_getvalue(&wait_c, &num_free_c);
    sem_getvalue(&wait_h, &num_free_h);
    
    // if I'm the only carbon, save my name in first spot
    if (num_free_c == 1) {
        combining_c1 = id;
    } 
    // else I'm second carbon, check if there's a hydrogen to make radical, else skip.
    else {
        combining_c2 = id;
        if  ((num_free_c == 0) && (num_free_h == 0)) {
            make_radical(combining_c1, combining_c2, combining_h, name);
            radicals++;
            sem_post(&wait_c);
            sem_post(&wait_c);
            sem_post(&wait_h);
            
        }
    }
    
    // give someone else a turn
    sem_post(&mutex);      


	return NULL;
}


/* 
 * Note: The function below need not be used, as the code for making radicals
 * could be located within h_ready and c_ready. However, it is perfectly
 * possible that you have a solution which depends on such a function
 * having a purpose as intended by the function's name.
 */
void make_radical(int c1, int c2, int h, char *maker)
{
#ifdef VERBOSE
	fprintf(stdout, "A ethynyl radical was made: c%03d  c%03d  h%03d\n",
		c1, c2, h);
#endif
    kosmos_log_add_entry(radicals, c1, c2, h, maker);
}


void wait_to_terminate(int expected_num_radicals) {
    /* A rather lazy way of doing it, for now. */
    sleep(MAX_KOSMOS_SECONDS);
    
    // gotta make sure I destroy them
    sem_destroy(&wait_h);
    sem_destroy(&wait_c);
    sem_destroy(&mutex);
    
    kosmos_log_dump();
    exit(0);
}
