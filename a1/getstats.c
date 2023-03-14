// V00848862
/* getstats.c 
 *
 * CSC 360, Summer 2022
 *
 * - If run without an argument, dumps information about the PC to STDOUT.
 *
 * - If run with a process number created by the current user, 
 *   dumps information about that process to STDOUT.
 *
 * Please change the following before submission:
 *
 * Author: Allison Tipan
 * Login:  allisontipan@uvic.ca 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Note: You are permitted, and even encouraged, to add other
 * support functions in order to reduce duplication of code, or
 * to increase the clarity of your solution, or both.
 */

void print_process_info(char * process_num) {
    // initialize char arrays with arbitrary number
    char path_name[100];
    char line_buffer[500];
    char linecopy[500];
    char no_file_name[500];
    char *token;
    
    // numbers and letters to look for inside a line to find offset
    char numbers[] = "1234567890";
    char letters[] = "abcdefghijklmnopqrstuvwxyz";
    int offset;
    
    int has_filename = 0;
    int ctxt_switches1;
    int ctxt_switches2;
    
    
    // concatenating pathname with given process number between proc and status
    strcpy(path_name,  "/proc/");
    strcat(path_name, process_num);
    strcat(path_name, "/status");
    
    // attempt to open file. If error, return.
    FILE *process_status = fopen(path_name, "r");
    if (process_status == NULL){
        printf("Process number %s not found.\n", process_num);
        exit(0);
    }
    
    // make sure pointer starts at beginning of file, print process number
    rewind(process_status);
    printf("Process number: %s\n", process_num);
    
    // while file has not reached the end, look at each line. Look for and print desired
    // process information and extract context switch numbers.
    while (!feof(process_status)){
        fgets(line_buffer, 500, process_status);
        strcpy(linecopy, line_buffer);
        token = strtok(line_buffer, ":");   
        if (strcmp(token, "Name") == 0){
            printf("%s", linecopy);
            strcpy(no_file_name, linecopy);
        }
        else if (strncmp(token, "Filename", 8) == 0){
            printf("%s", linecopy);
            // if filename was found, set flag to true
            has_filename = 1;
        }
        else if (strcmp(token, "Threads") == 0){
            // if filename was not found (flag = false), then find offset and extract/print 
            // saved value from "Name"
            if (!has_filename){
                token = strtok(no_file_name, ":"); 
                token = strtok(NULL, " ");
                
                offset = strcspn(token, letters);
                printf("Filename (if any): %s", token+offset);
            }
            printf("%s", linecopy);
            
        }
        else if (strcmp(token, "voluntary_ctxt_switches") == 0){
            token = strtok(NULL, " ");
            offset = strcspn(token, numbers);
            ctxt_switches1 = (int)atof(token+offset);
        }
        else if (strcmp(token, "nonvoluntary_ctxt_switches") == 0){
            token = strtok(NULL, " ");
            offset = strcspn(token, numbers);
            ctxt_switches2 = (int)atof(token+offset);
            break;
        }
    }
    
    // print the total of context switches
    printf("Total context switches: %d\n", ctxt_switches2+ctxt_switches1);
    
    fclose(process_status); 
    return;
    
} 


void print_full_info() {
    char line_buffer[500];
    char linecopy[500];
    char *token;
    int days, hours, minutes, seconds;
    
    // check if file was opened correctly
    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (cpuinfo == NULL){
        printf("File cpuinfo did not open correctly.\n");
        exit(0);
    }
    
    // make sure pointer starts at beginning of file
    // look through the length of one processor information block (less than 15 lines)
    // to find model name and cpu cores.
    rewind(cpuinfo);
    for (int n=0; n<15; n++){
        fgets(line_buffer, 500, cpuinfo);
        strcpy(linecopy, line_buffer);
        token = strtok(line_buffer, " ");   
        if (strcmp(token, "model") == 0){
            token = strtok(NULL, " ");
            if (strncmp(token, "name", 4) == 0){
                printf("%s", linecopy);
            }
        }
        else if (strcmp(token, "cpu") == 0){
            token = strtok(NULL, " ");
            if (strncmp(token, "cores", 4) == 0){
                printf("%s", linecopy);
                break;
            }
        }
    }
    fclose(cpuinfo);  
    
    
    
    FILE *version = fopen("/proc/version", "r");    
    if (version == NULL){
        printf("File version did not open correctly.\n");
        exit(0);
    }
    
    // just print "version"'s first line
    rewind(version);
    fgets(line_buffer, 500, version);
    strcpy(linecopy, line_buffer);
    printf("%s", linecopy);

    fclose(version);            
    
    
                 
    FILE *meminfo = fopen("/proc/meminfo", "r");
    if (meminfo == NULL){
        printf("File meminfo did not open correctly.\n");
        exit(0);
    }
    
    // Memtotal is the first line. Don't need to iterate through whole file.
    rewind(meminfo);
    fgets(line_buffer, 500, meminfo);
    strcpy(linecopy, line_buffer);
    token = strtok(line_buffer, " ");   
    if (strncmp(token, "MemTotal", 8) == 0){
        printf("%s", linecopy);
    }   
    fclose(meminfo);                
             
 
    
    FILE *uptime = fopen("/proc/uptime", "r");
    if (uptime == NULL){
        printf("File uptime did not open correctly.\n");
        exit(0);
    }
    
    // retrieves first value in uptime and calculates the days/hours/minutes/seconds
    rewind(uptime);
    fgets(line_buffer, 500, uptime);
    token = strtok(line_buffer, " ");
    int total_uptime = (int)atof(token);
    if (total_uptime >= 86400){
        days = (int)(total_uptime / 86400) ;
        total_uptime = total_uptime % 86400 ;
    }
    if ((total_uptime >= 3600) && (total_uptime < 86400)){
        hours = (int)(total_uptime / 3600) ;
        total_uptime = total_uptime % 3600 ;
    }
    if ((total_uptime >= 60) && (total_uptime < 3600)){
        minutes = (int)(total_uptime / 60) ;
        total_uptime = total_uptime % 60 ;
    }
    if (total_uptime < 60){
        seconds = total_uptime;
    }
    printf("Uptime: %d days, %d hours, %d minutes, %d seconds\n", days, hours, minutes, seconds);
    fclose(uptime);
    
    return;
}


int main(int argc, char ** argv) {  
    if (argc == 1) {
        print_full_info();
    } else {
        print_process_info(argv[1]);
    }
}
