// Allison Tipan V00848862
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "disk.h"


int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename = NULL;
    char *filename  = NULL;
    FILE *f;
    
    directory_entry_t dir_entry;
    int found = 0;
    int index;
    int temp;
    char *buff; 
    int bytes_read = 0;
    int *fat_entries;
    int last_block_val = 0xffffffff;
    
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            filename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL || filename == NULL) {
        fprintf(stderr, "usage: cat360fs --image <imagename> " \
            "--file <filename in image>");
        exit(1);
    }

    
    f = fopen(imagename, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening file \n");
        exit(1);
    }
    
    
    // go to the beginning of file, read the first n bytes of the superblock into sb
    fseek(f, 0, SEEK_SET);
    bytes_read = fread(&sb, sizeof(sb), 1, f);
    if (bytes_read == 0) { 
            fprintf(stderr, "Problem reading file \n");
            fclose(f);
            exit(1);
        }
	
    // convert endian order of each attribute
    sb.block_size = htons(sb.block_size);
    sb.num_blocks = htonl(sb.num_blocks);
    sb.fat_start = htonl(sb.fat_start);
    sb.fat_blocks = htonl(sb.fat_blocks);
    sb.dir_start = htonl(sb.dir_start);
    sb.dir_blocks = htonl(sb.dir_blocks);
	
    
    // calculate start of the "root directory entries" section and initialize space for fat entries
    int dir_start = sb.block_size + (sb.fat_blocks * sb.block_size);
    int fat_start = sb.block_size;
    fat_entries = (int*) calloc(sb.num_blocks, sizeof(int));
    
    // Go to start of FAT section and read contents of FAT into array. Check for errors
    fseek(f, fat_start, SEEK_SET);
    for (int x = 0; x < sb.num_blocks; x++){
        bytes_read = fread(&fat_entries[x], 4, 1, f);
        if (bytes_read == 0) { 
            fprintf(stderr, "Problem reading fat entries \n");
            free(fat_entries);
            fclose(f);
            exit(1);
        }
        fat_entries[x] = htonl(fat_entries[x]);
    }
    
    // position of f shoudl be at root directory. but to be safe, go to root dir anyways
    fseek(f, dir_start, SEEK_SET);
    
    
    // look through each directory entry (in case entries aren't all together at the beginning)
    for (int iterations = 0; iterations < MAX_DIR_ENTRIES; iterations++) {
        
        // get directory entry
        fread(&dir_entry, sizeof(dir_entry), 1, f);
        
        // if directory is a file, convert contents endianness
        if (dir_entry.status == DIR_ENTRY_NORMALFILE){
            dir_entry.start_block = htonl(dir_entry.start_block);
            dir_entry.num_blocks = htonl(dir_entry.num_blocks);
            dir_entry.file_size = htonl(dir_entry.file_size);
            
            // if file is found, stop searching, set flag
            if (strcmp(dir_entry.filename, argv[4]) == 0) {
                found = 1;
                break;
            }
        }
        else {
            // pass, keep looking
        }
    }
    
    // if flag is false, file was not found. Print message and exit
    if (found == 0){
        fprintf(stderr, "File not found \n");
        free(fat_entries);
        fclose(f);
        exit(1);
    }
    
    
    // initialize a single block space as buffer and get first index
    buff = (char*) calloc(sb.block_size, sizeof(char));
    index = dir_entry.start_block;
    
    while (1) {
        // get value at index of FAT entry, use it to go to data block 
        temp = fat_entries[index];
        fseek(f, index * sb.block_size, SEEK_SET);
        
        // try read a block, if reading error, exit
        bytes_read = fread(buff, sizeof(char), sb.block_size, f);
        if (bytes_read == 0) { 
            fprintf(stderr, "Problem freading  \n");
            free(fat_entries);
            free(buff);
            fclose(f);
            exit(1);
        }
                
        // write same amount of bytes read to stdout
        fwrite(buff, sizeof(char), bytes_read, stdout);
        if (temp == last_block_val) {
            break;
        }
        // change index to next fat entry
        index = temp;
    }
    
    free(fat_entries);
    free(buff);
	fclose(f);
    
    return 0;                       
    
}