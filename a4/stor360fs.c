// Allison Tipan V00848862
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "disk.h"


/*
 * Based on http://bit.ly/2vniWNb
 */
void pack_current_datetime(unsigned char *entry) {
    assert(entry);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    unsigned short year   = tm.tm_year + 1900;
    unsigned char  month  = (unsigned char)(tm.tm_mon + 1);
    unsigned char  day    = (unsigned char)(tm.tm_mday);
    unsigned char  hour   = (unsigned char)(tm.tm_hour);
    unsigned char  minute = (unsigned char)(tm.tm_min);
    unsigned char  second = (unsigned char)(tm.tm_sec);

    year = htons(year);

    memcpy(entry, &year, 2);
    entry[2] = month;
    entry[3] = day;
    entry[4] = hour;
    entry[5] = minute;
    entry[6] = second; 
}


int next_free_block(int *FAT, int max_blocks) {
    assert(FAT != NULL);

    int i;

    for (i = 0; i < max_blocks; i++) {
        if (FAT[i] == FAT_AVAILABLE) {
            return i;
        }
    }

    return -1;
}


int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename  = NULL;
    char *filename   = NULL;
    char *sourcename = NULL;
    FILE *f;
    FILE *sourcef;
    directory_entry_t dir_entry;
    directory_entry_t new_dir_entry;
    int count_blocks = 0;
    int free_blocks = 0;
    int resv_blocks = 0;
    int alloc_blocks = 0;
    int bytes_read = 0;
    int bytes_written = 0;
    int first_empty_dir = 0;
    int last_block_val = 0xffffffff;
    int source_size;
    char *buff;
    int *fat_entries;
    int block_to_write;
    int write;
        
    
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            filename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--source") == 0 && i+1 < argc) {
            sourcename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL || filename == NULL || sourcename == NULL) {
        fprintf(stderr, "usage: stor360fs --image <imagename> " \
            "--file <filename in image> " \
            "--source <filename on host>\n");
        exit(1);
    }

    f = fopen(imagename, "r+");
    if (f == NULL) {
        fprintf(stderr, "Error opening file \n");
        exit(1);
    }
    
    sourcef = fopen(sourcename, "r");
    if (sourcef == NULL) {
        fprintf(stderr, "Error opening source file \n");
        fclose(f);
        exit(1);
    }
    
    // go to the beginning of file, read the first n bytes of the superblock into sb
    fseek(f, 0, SEEK_SET);
    bytes_read = fread(&sb, sizeof(sb), 1, f);
    if (bytes_read == 0) { 
            fprintf(stderr, "Problem reading superblock\n");
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
    
    
    // calculate start of the "root directory entries" and "FAT" sections. Initialize space for fat entries array
    int dir_start = sb.block_size + (sb.fat_blocks * sb.block_size);
    int fat_start = sb.block_size;
    fat_entries = (int*) calloc(sb.num_blocks, sizeof(int));
    int first_empty_fat = 0;
    
    // Go to start of FAT section and read contents of FAT into array
    fseek(f, fat_start, SEEK_SET);
    for (int x = 0; x < sb.num_blocks; x++){
        fread(&fat_entries[x], 4, 1, f);
        fat_entries[x] = htonl(fat_entries[x]);
        
        // calculate number of free, reserved, and allocated blocks while we're at it
        if (fat_entries[x] == 0) {
            if (first_empty_fat == 0) {
                first_empty_fat = x;
            }
            free_blocks++;
        }
        else if (fat_entries[x] == 1) {
            resv_blocks++;
        }
        else {
            alloc_blocks++;
        }
    }
    
    // figure out size of the source file 
    fseek(sourcef, 0, SEEK_END);
    source_size = ftell(sourcef);
    
    // if there are not enough free blocks to accomodate size of source file, print message and exit
    if (free_blocks * sb.block_size < source_size) {
        fprintf(stderr, "There is not enough room to store the file on the disk image \n");
        fclose(sourcef);
        fclose(f);
        free(fat_entries);
        exit(1);
    }
    
    
    // position of f should be at root directory. but to be safe go to start of the "root directory entries" section
    fseek(f, dir_start, SEEK_SET);
    
    // look through each directory entry (in case entries aren't all together at the beginning)
    for (int iterations = 0; iterations < MAX_DIR_ENTRIES; iterations++) {
        
        // get directory entry
        fread(&dir_entry, sizeof(dir_entry), 1, f);
        
        // if directory is a file, convert contents endianness, convert modify time 
        if (dir_entry.status == DIR_ENTRY_NORMALFILE){
            dir_entry.start_block = htonl(dir_entry.start_block);
            dir_entry.num_blocks = htonl(dir_entry.num_blocks);
            dir_entry.file_size = htonl(dir_entry.file_size);
            
            // if file is found, print message and exit
            if (strcmp(dir_entry.filename, argv[4]) == 0) {
                fprintf(stderr, "File already exists in file image \n");
                fclose(sourcef);
                fclose(f);
                free(fat_entries);
                exit(1);
            }
        }
        else {
            // save index of the first empty directory slot
            if (first_empty_dir == 0){
                first_empty_dir = iterations;
            }
        }
    }
    
    
    // setup new directory's info
    new_dir_entry.status = DIR_ENTRY_NORMALFILE;
    new_dir_entry.start_block = ntohl(first_empty_fat);
    pack_current_datetime(new_dir_entry.create_time); 
    pack_current_datetime(new_dir_entry.modify_time);
    strcpy(new_dir_entry.filename, argv[4]);
    
    
    // initialize a single block space as buffer then go to beginning of source file
    buff = (char*) calloc(sb.block_size, sizeof(char));
    fseek(sourcef, 0, SEEK_SET);
    
    // get index of free fat (which equals free data block index)
    block_to_write = next_free_block(fat_entries, sb.num_blocks);
    
    // writing to data loop
    while (1) {
        
        // go to free data block and read 1 block from source file, write to image, check for errors
        fseek(f, sb.block_size*block_to_write , SEEK_SET);
        bytes_read = fread(buff, sizeof(char), sb.block_size, sourcef);
        bytes_written = fwrite(buff, sizeof(char), bytes_read, f);
        if (bytes_read == 0 || bytes_written == 0) { 
            fprintf(stderr, "Problem reading or writting data \n");
            fclose(sourcef);
            fclose(f);
            free(fat_entries);
            free(buff);
            exit(1);
        }
        
        // update block count and go to the fat entry
        count_blocks++;
        fseek(f, sb.block_size + (4*block_to_write) , SEEK_SET);
        
        // if all data was written, break
        if (bytes_read < sb.block_size) {
            break;
        }
        
        // update in fat_entries that the block is no longer available
        fat_entries[block_to_write] = block_to_write;
        
        // get the next free block value from fat entries and convert endianness then write to current fat entry
        block_to_write = next_free_block(fat_entries, sb.num_blocks);
        write = ntohl(block_to_write);
        fwrite(&write, sizeof(write), 1, f);
    }
    
    // write last block entry in FAT
    fwrite(&last_block_val, sizeof(last_block_val), 1, f);
    
    
    // finish updating new directory info
    new_dir_entry.num_blocks = ntohl(count_blocks);
    new_dir_entry.file_size = ntohl(source_size);
    
    
    // go to Dir empty, WRITE DIR ENTRY
    int dir_entry_addr = sb.block_size + (sb.block_size*sb.fat_blocks) + (first_empty_dir * 64);
    fseek(f, dir_entry_addr, SEEK_SET);
    
    fwrite(&new_dir_entry, sizeof(new_dir_entry), 1, f);
    
    fclose(sourcef);
    fclose(f);
    free(fat_entries);
    free(buff);
    return 0; 
}
