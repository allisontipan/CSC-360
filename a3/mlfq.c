// Allison Tipan V00848862
/*
 * UVic CSC 360, Summer 2022
 * This code copyright 2022: Roshan Lasredo, Mike Zastre, David Clark
 *
 * Assignment 3
 * --------------------
 * 	Simulate a Multi-Level Feedback Queue with `3` Levels/Queues each 
 * 	implementing a Round-Robin scheduling policy with a Time Quantum
 * 	of `2`, `4` and `8` resp, and including a boost mechanism.
 * 
 * Input: Command Line args
 * ------------------------
 * 	./mlfq <input_test_case_file>
 * 	e.g.
 * 	     ./mlfq test1.txt
 * 
 * Input: Test Case file
 * ---------------------
 * 	Each line corresponds to an instruction and is formatted as:
 *
 * 	<event_tick>,<task_id>,<burst_time>
 * 
 * 	NOTE: 
 * 	1) All times are represented as `whole numbers`.
 * 	2) Special Case:
 * 	     burst_time =  0 -- Task Creation
 * 	     burst_time = -1 -- Task Termination
 * 
 * 
 * Assumptions: (For Multi-Level Feedback Queue)
 * -----------------------
 * 	1) On arrival of a Task with the same priority as the current 
 * 		Task, the current Task is not preempted.
 * 	2) A Task on being preempted is added to the end of its queue.
 * 	3) Arrival tick, Burst tick and termination tick for the same  
 * 		Task will never overlap. But the arrival/exit of one  
 * 		Task may overlap with another Task.
 * 	4) Tasks will be labelled from 1 to 10.
 * 	5) The event_ticks in the test files will be in sorted order.
 * 	6) Once a Task is assigned a queue, it will always continue to 
 * 		run in that queue for any new future bursts (Unless further 
 * 		demoted, or returned to queue 1 by a boost).
 * 	7) Task termination instruction will always come after the 
 * 		Task completion for the given test case.
 * 	8) Task arrival/termination/boosting does not consume CPU cycles.
 * 	9) A task is enqueued into one of the queues only if it requires
 * 		CPU bursts.
 * 	
 * Output:
 * -----------------------
 * 	NOTE: Do not modify the formatting of the print statements.
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdbool.h>

/*
 * A queue structure provided to you by the teaching team. This header
 * file contains the function prototypes; the queue routines are
 * linked in from a separate .o file (which is done for you via
 * the `makefile`).
 */
#include "queue.h"


/* 
 * Some constants related to assignment description.
 */
#define MAX_INPUT_LINE 100
#define MAX_TASKS 10
#define BOOST_INTERVAL 25
const int QUEUE_TIME_QUANTUMS[] = { 2, 4, 8 };


/*
 * Here are variables that are available to methods. Given these are 
 * global, you need not pass them as parameters to functions. 
 * However, you must be careful while initializing/setting these
 * global variables.
 */
Queue_t *queue_1;
Queue_t *queue_2;
Queue_t *queue_3;
Task_t task_table[MAX_TASKS];
Task_t *current_task;
int remaining_quantum = 0;		// Remaining Time Quantum for the current task


/*
 * Function: validate_args
 * -----------------------
 *  Validate the input command line args.
 *
 *  argc: Number of command line arguments provided
 *  argv: Command line arguments
 */
void validate_args(int argc, char *argv[]) { 
	if(argc != 2) {
		fprintf(stderr, "Invalid number of input args provided! Expected 1, received %d\n", argc - 1);
		exit(1);
	}
}


/*
 * Function: initialize_vars
 * -------------------------
 *  Initialize the three queues.
 */
void initialize_vars() {  
	queue_1 = init_queue();
	queue_2 = init_queue();
	queue_3 = init_queue();
}

/*
 * Function: read_instruction
 * --------------------------
 *  Reads a single line from the input file and stores the 
 *  appropriate values in the instruction pointer provided. In case
 *  `EOF` is encountered, the `is_eof` flag is set.
 *
 *  fp: File pointer to the input test file
 *  instruction: Pointer to store the read instruction details
 */
void read_instruction(FILE *fp, Instruction_t *instruction) {  
	char line[MAX_INPUT_LINE];
	
	if(fgets(line, sizeof(line), fp) == NULL) {
		instruction->event_tick = -1;
		instruction->is_eof = true;
		return;
	}

	int vars_read = sscanf(line, "%d,%d,%d", &instruction->event_tick, 
	&instruction->task_id, &instruction->burst_time);
	instruction->is_eof = false;

	if(vars_read == EOF || vars_read != 3) {
		fprintf(stderr, "Error reading from the file.\n");
		exit(1);
	}

	if(instruction->event_tick < 0 || instruction->task_id < 0) {
		fprintf(stderr, "Incorrect file input.\n");
		exit(1);
	}
}




/*
 * Function: get_queue_by_id
 * -------------------------
 *  Returns the Queue associated with the given `queue_id`.
 *
 *  queue_id: Integer Queue identifier.
 */
Queue_t *get_queue_by_id(int queue_id) {  
	switch (queue_id) {
		case 1:
			return queue_1;

		case 2:
			return queue_2;

		case 3:
			return queue_3;
	}
	return NULL;
}



/*
 * Function: handle_instruction
 * ----------------------------
 *  Processes the input instruction, depending on the instruction
 *  type:
 *      a. New Task (burst_time == 0)
 *      b. Task Completion (burst_time == -1)
 *      c. Task Burst (burst_time == <int>)
 *
 *  NOTE: 
 *	a. This method performs NO task scheduling, NO Preemption and NO
 *  	Updation of Task priorities/levels. These tasks would be   
 *		handled by the `scheduler`.
 *	b. A task once demoted to a level, retains that level for all 
 *		future bursts unless it is further demoted or boosted.
 *
 *  instruction: Input instruction
 *  tick: Clock tick (ONLY For Print statements)
 */
void handle_instruction(Instruction_t *instruction, int tick) {  
	
    int task_id = instruction->task_id;
   
    Task_t *temp;
    temp = &task_table[task_id-1];
    
	if (instruction->burst_time == 0) { 
		// New Task Arrival
        
        temp->id = task_id;
        
		printf("[%05d] id=%04d NEW\n", tick, task_id);
	
	} else if(instruction->burst_time == -1) { 
		// Task Termination
		int waiting_time;
		int turn_around_time;

        waiting_time = temp->total_wait_time;
        turn_around_time = temp->total_execution_time + temp->total_wait_time;; 

		printf("[%05d] id=%04d EXIT wt=%d tat=%d\n", tick, task_id, 
			waiting_time, turn_around_time);
        

	} else {
		// update bursts for the task
        temp->burst_time = instruction->burst_time;
        temp->remaining_burst_time = instruction->burst_time;
        
        // if brand new, send to first queue, else put it back in its previous one
        if (temp->current_queue == 0) {
            temp->current_queue = 1;
            enqueue(queue_1, temp);
        }   
        else {
            enqueue(get_queue_by_id(temp->current_queue), temp);
        }
	}
}



/*
 * Function: peek_priority_task
 * ----------------------------
 *  Returns a reference to the Task with the highest priority.
 *  Does NOT dequeue the task.
 */
Task_t *peek_priority_task() {
    
    // take the first task from the first non-empty queue
    if (!is_empty(queue_1)) {
        return queue_1->start;
    }
    else if (!is_empty(queue_2)) {
        return queue_2->start;
    }        
    else if (!is_empty(queue_3)) {
        return queue_3->start;
    }
    else {
        return NULL;
    }
}



/*
 * Function: decrease_task_level
 * -----------------------------
 *  Updates the task to lower its level(Queue) by 1.
 */
void decrease_task_level(Task_t *task) { 
	task->current_queue = task->current_queue == 3 ? 3 : task->current_queue + 1;
}
/*
 * Function: boost
 * -----------------------------
 *  If the current tick is a multiple of the BOOST_INTERVAL, perform a boost
 *  on all tasks in Queue 3, followed by a boost on all tasks in Queue
 *  2.  A boost is done by dequeuing the task from its current queue 
 *  and queuing it into Queue 1.  At the end of this process, all tasks
 *  with remaining CPU bursts should be in Queue 1.  The current task
 *  should be unaffected, except that its remaining quantum should be
 *  set to a maximum of 2 (or left unchanged if it's less than two). 
 *  Boosts do not take CPU time.
 */

void boost(int tick) {
	
	if (tick % BOOST_INTERVAL != 0) {
        return;
    }
    
    if (current_task != NULL) {
        current_task->current_queue = 1;
    }
    Task_t *temp;
	// Conduct boost
    while (!is_empty(queue_3)) {
        temp = dequeue(queue_3);
        temp->current_queue = 1;
        enqueue(queue_1, temp);
    }
    
    while (!is_empty(queue_2)) {
        temp = dequeue(queue_2);
        temp->current_queue = 1;
        enqueue(queue_1, temp);
    }
	
    if (remaining_quantum > 2) {
        remaining_quantum = 2;
    }
    
	printf("[%05d] BOOST\n", tick);
}

/*
 * Function: scheduler
 * -------------------
 *  Schedules the task having the highest priority to be the current 
 *  task. Also, for the currently executing task, decreases the task    
 *	level on completion of the current time quantum.
 *
 *  NOTE:
 *  a. The task to be currently executed is `dequeued` from one of the
 *  	queues.
 *  b. On Pre-emption of a task by another task, the preempted task 
 *  	is `enqueued` to the end of its associated queue.
 */
void scheduler() {
    
  Task_t *priority_task = peek_priority_task();
    
    // there was a current task ... 
    if (current_task != NULL) {
        
        // ... and it's not finished yet
        if (current_task->remaining_burst_time > 0) {
            
            // ... and still has quantum left
            if (remaining_quantum > 0) {
                
                // but another task is more important, so step aside peasant. (put current task back in its queue and retrieve more important task) 
                if (priority_task != NULL && priority_task->current_queue < current_task->current_queue) {                                                
                    enqueue(get_queue_by_id(current_task->current_queue), current_task);
                    current_task = dequeue(get_queue_by_id(priority_task->current_queue));                    
                    remaining_quantum = QUEUE_TIME_QUANTUMS[current_task->current_queue-1];
                }
                
                // all me, all day long
                else {                                      
                    return;
                }
            }
            // ...but no more quantum so we'll adjust its queue level and slot it back into current task.
            else {          
                decrease_task_level(current_task);
                enqueue(get_queue_by_id(current_task->current_queue), current_task);
                Task_t *priority_task = peek_priority_task();
                current_task = dequeue(get_queue_by_id(priority_task->current_queue));
                remaining_quantum = QUEUE_TIME_QUANTUMS[current_task->current_queue-1];
            }
        }
        
        // .. and I've finished my burst
        else {   
            
            // ... let's give the next guy a turn.
            if (priority_task != NULL) {
                current_task = dequeue(get_queue_by_id(priority_task->current_queue));
                remaining_quantum = QUEUE_TIME_QUANTUMS[current_task->current_queue-1];
            }
            
            // .. but no task in queue, so make current task NULL
            else {   
                current_task = NULL;
                 
            }
        }
    }
    
    // no current task...
    else {                                                               
        // .. but no task in queue, so we'll do nothing
        if (priority_task == NULL) {
            return;
        }
                                
        current_task = dequeue(get_queue_by_id(priority_task->current_queue));
        remaining_quantum = QUEUE_TIME_QUANTUMS[current_task->current_queue-1];
   }

}



/*
 * Function: execute_task
 * ----------------------
 *  Executes the current task (By updating the associated remaining
 *  times). Sets the current_task to NULL on completion of the
 *	current burst.
 *
 *  tick: Clock tick (ONLY For Print statements)
 */
void execute_task(int tick) {
	if (current_task != NULL) {
        
        // updates to execution and remaining burst times of the current task and quantum for the single tick 
        current_task->total_execution_time ++;
        current_task->remaining_burst_time --;
        remaining_quantum --;
        
		printf("[%05d] id=%04d req=%d used=%d queue=%d\n", tick, 
			current_task->id, current_task->burst_time, 
			(current_task->burst_time - current_task->remaining_burst_time), 
			current_task->current_queue);
		
		if(current_task->remaining_burst_time == 0) {
			current_task = NULL;
		}

	} else {
		printf("[%05d] IDLE\n", tick);
	}
}



/*
 * Function: update_task_metrics
 * -----------------------------
 * 	Increments the waiting time/execution time for the tasks 
 * 	that are currently scheduled (In the queue). These values would  
 * 	be later used for the computation of the task waiting time and  
 *	turnaround time.
 */
void update_task_metrics() {
    
    Task_t *temp; 
    
    // for each non-empty queue, update each task's total_wait_time. 
    if (!is_empty(queue_1)) {
        temp = queue_1->start;
        while (temp != NULL) {
            temp->total_wait_time ++;
            temp = temp->next;
        }
    }
        
    if (!is_empty(queue_2)) {
        temp = queue_2->start;
        while (temp != NULL) {
            temp->total_wait_time ++;
            temp = temp->next;
        }
    }

    if (!is_empty(queue_3)) {
        temp = queue_3->start;
        while (temp != NULL) {
            temp->total_wait_time ++;
            temp = temp->next;
        }
    }

}



/*
 * Function: main
 * --------------
 * argv[1]: Input file/test case.
 */
int main(int argc, char *argv[]) {
	int tick = 1;
	int is_inst_complete = false;
	
	validate_args(argc, argv);
	initialize_vars();

	FILE *fp = fopen(argv[1], "r");

	if(fp == NULL) {
		fprintf(stderr, "File \"%s\" does not exist.\n", argv[1]);
		exit(1);
	}

	Instruction_t *curr_instruction = (Instruction_t*) malloc(sizeof(Instruction_t));
	
	// Read First Instruction
	read_instruction(fp, curr_instruction);

	if(curr_instruction->is_eof) {
		fprintf(stderr, "Error reading from the file. The file is empty.\n");
		exit(1);
	}
    
	while(true) {
		while(curr_instruction->event_tick == tick) {
            handle_instruction(curr_instruction, tick);
            
			// Read Next Instruction
			read_instruction(fp, curr_instruction);
			if(curr_instruction->is_eof) {
				is_inst_complete = true;
			}
		}
        
		boost(tick);	
        
		scheduler();
        
		update_task_metrics();

		execute_task(tick);

		
		if(is_inst_complete && is_empty(queue_1) && is_empty(queue_2) && is_empty(queue_3) && current_task == NULL) {
			break;
		}

		tick++;
	}

	fclose(fp);
	deallocate(curr_instruction);
	deallocate(queue_1);
	deallocate(queue_2);
	deallocate(queue_3);
}

