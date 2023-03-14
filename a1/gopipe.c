// V00848862
/* gopipe.c
 *
 * CSC 360, Summer 2022
 *
 * Execute up to four instructions, piping the output of each into the
 * input of the next.
 *
 * Please change the following before submission:
 *
 * Author: Allison Tipan
 * Login:  allisontipan@uvic.ca 
 */


/* Note: The following are the **ONLY** header files you are
 * permitted to use for this assignment! */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <wait.h>
#include <stdio.h>

// this function performs read() to get the command and arguments from stdin. Then tokenizes the string and adds them to the input_args array accordingly.
int get_input_commands(char input_buffer[], char *input_args[], char *token){
    int  token_index;
    
    read(0, input_buffer, 80);
    
    if (input_buffer[0] == '\n'){                                               // if no input was given, break and return 1
        return 1;
    }
    
    // lines 33 to 42 are taken and adapted from appendix_d.c lines(38-47)
    if (input_buffer[strlen(input_buffer) - 1] == '\n') {                       
        input_buffer[strlen(input_buffer) - 1] = '\0';                          // if last char of line was newline, change to \0 null character
    }

    token_index = 0;                                                            // token index 
    token = strtok(input_buffer, " ");                                          // tokenize 'input_buffer' line on spaces, and store resulting pointer-to-first-token in 'token'
    while (token != NULL && token_index < 8) {                                  // while token pointer isn't null, and token index is less than max
        input_args[token_index] = token;                                        // char * array called 'input_args' at [token index] now equals 'token' pointer token
        token_index++;                                                          // increment token index
        token = strtok(NULL, " ");                                              // 'token' now points to next token 
    }
    input_args[token_index] = 0;
    
    
    return 0;
}


int main() {
    char input_buffer1[80];
    char input_buffer2[80];
    char input_buffer3[80];
    char input_buffer4[80];
    char *input_args1[8];
    char *input_args2[8];
    char *input_args3[8];
    char *input_args4[8];
    char *token1 ="rand1";
    char *token2 ="rand2";
    char *token3 ="rand3";
    char *token4 ="rand4";
    
    // lines 64-70 are taken and adapted from appendix_c.c lines(18-21)
    char *envp[] = { 0 };
    int status;
    int pid_1, pid_2, pid_3, pid_4;
    int pipe1[2];
    int pipe2[2];
    int pipe3[2];
    
    int flag = 0;
    int input_finished;
    
    // calls function to retrieve input from read(), if nothing was entered, returns 1.
    input_finished = get_input_commands(input_buffer1, input_args1, token1);
    
    // if previous call returned 0, attempt to get more commands up to a 4th input

    if (!input_finished){
        flag=1;
        input_finished = get_input_commands(input_buffer2, input_args2, token2); 
    }

    if (!input_finished){
        flag = 2;
        input_finished = get_input_commands(input_buffer3, input_args3, token3);
    }

    
    if (!input_finished){
        flag = 3;
        input_finished = get_input_commands(input_buffer4, input_args4, token4);


        if (!input_finished){
            flag = 4;                                                              // automatically sets fourth flag if there was a command entered in fourth read()
        }
    }

    
    // at this point I have tokenized all inputs from stdin, and setup char *input_args[]
    
    
    // the following lines for piping structure are taken and adapted from appendix_c.c lines(34-48)
    pipe(pipe1);                                                                   // creates pipe, [0] = read from, [1] = write to/IN
    if (((pid_1 = fork()) == 0) && (flag>=1)) {                                    // if child process
        if (flag>1){                                                               // if more commands, connects: pipe's [1] (write to/IN PIPE) <--to-- 1 (stdout)
            
            dup2(pipe1[1], 1);                                                     
            
            close(pipe1[0]);                                                       // close pipe's [0] read from/OUT PIPE
        }
        execve(input_args1[0], input_args1, envp);                                 // replace itself with new program 
    }
    
    
    pipe(pipe2);
    if (((pid_2 = fork()) == 0) && (flag>=2)) {                                    // if other child process
        dup2(pipe1[0], 0);                                                         // connect: pipe's [0] (read from/OUT PIPE) --to--> 0 (stdin?)
        close(pipe1[1]);                                                           // close pipe's [0] read from/OUT PIPE
        if (flag>2){
            
            dup2(pipe2[1], 1);                                                     // if there are more arguments, connect: nextpipe's [1] <--to-- 1 (stdout)
            
            close(pipe2[0]);                                                       // close newpipe's [0] read from/OUT PIPE
        }
        execve(input_args2[0], input_args2, envp);                                 // replace itself with new program
    }
    
    close(pipe1[0]);
    close(pipe1[1]);
    
    pipe(pipe3);
    if (((pid_3 = fork()) == 0) && (flag>=3)) {                                      
        dup2(pipe2[0], 0);                                                        
        close(pipe2[1]); 
        if (flag>3){
            
            dup2(pipe3[1], 1);
            
            close(pipe3[0]);                                                         
        }                                                        
        execve(input_args3[0], input_args3, envp);                                    
    }
    close(pipe2[0]);
    close(pipe2[1]);
    
    if (((pid_4 = fork()) == 0) && (flag==4)) {                                             
        dup2(pipe3[0], 0);                                                         
        close(pipe3[1]);                                                            
        execve(input_args4[0], input_args4, envp);                                    
    }
    
    
    // the following lines are taken and adapted from appendix_c.c lines(60/61 and 64/68) 
    // close everything
    
    
    close(pipe3[0]);
    close(pipe3[1]);
   
    
    // wait for all children to finish
    waitpid(pid_1, &status, 0);
    waitpid(pid_2, &status, 0); 
    waitpid(pid_3, &status, 0); 
    waitpid(pid_4, &status, 0); 
    
    
    exit(0);
    
}


