// Allison Tipan V00848862
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
    FILE  *f;
    
    int bytes_read;
    int fat_entry;
    int free_blocks = 0;
    int resv_blocks = 0;
    int alloc_blocks = 0;
    
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL)
    {
        fprintf(stderr, "usage: stat360fs --image <imagename>\n");
        exit(1);
    }
    
    
    // open file and check for error
    f = fopen(imagename, "r");
    if (f == NULL) { 
        fprintf(stderr, "Error opening file\n");
        exit(1);
    }
    
    
    // go to the beginning of file, read the first n bytes of the superblock into sb, check for errors
    fseek(f, 0, SEEK_SET);
    bytes_read = fread(&sb, sizeof(sb), 1, f);
    if (bytes_read == 0) { 
            fprintf(stderr, "Problem reading file\n");
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
    
    
    // set file position to beginning of FAT section 
    fseek(f, sb.block_size, SEEK_SET);
    
    // iterate through FAT entries and count number of free/reserved/allocated blocks
    for (int index = 0; index < sb.num_blocks; index++){
        fread(&fat_entry, 4, 1, f);
        
        fat_entry = htonl(fat_entry);
          
        if (fat_entry == 0) {
            free_blocks++;
        }
        else if (fat_entry == 1) {
            resv_blocks++;
        }
        else {
            alloc_blocks++;
        }
    }
    
    // print formatted output stats
    printf("=================================================================\n");
    printf("%s (%s)\n\n", sb.magic, argv[2]);
    printf("-------------------------------------------------\n");
    printf("  Bsz   Bcnt  FATst FATcnt  DIRst DIRcnt  \n");
    printf("%5d%7d%7d%7d%7d%7d\n\n", sb.block_size, sb.num_blocks, sb.fat_start, sb.fat_blocks, sb.dir_start, sb.dir_blocks);
    printf("-------------------------------------------------\n");
    printf(" Free   Resv  Alloc \n");
    printf("%5d%7d%7d\n\n\n\n", free_blocks, resv_blocks, alloc_blocks);
    
    
    fclose(f);
    
    return 0; 
}
