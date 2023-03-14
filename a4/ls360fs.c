// Allison Tipan V00848862
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "disk.h"


char *month_to_string(short m) {
    switch(m) {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "Jun";
    case 7: return "Jul";
    case 8: return "Aug";
    case 9: return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "?!?";
    }
}


void unpack_datetime(unsigned char *time, short *year, short *month, 
    short *day, short *hour, short *minute, short *second)
{
    assert(time != NULL);

    memcpy(year, time, 2);
    *year = htons(*year);

    *month = (unsigned short)(time[2]);
    *day = (unsigned short)(time[3]);
    *hour = (unsigned short)(time[4]);
    *minute = (unsigned short)(time[5]);
    *second = (unsigned short)(time[6]);
}


int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename = NULL;
    FILE *f;
    
    directory_entry_t dir_entry;
    int bytes_read;
    short year;
    short month;
    short day;
    short hour;
    short minute;
    short second;
    
    
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        }
    }
    
    

    if (imagename == NULL)
    {
        fprintf(stderr, "usage: ls360fs --image <imagename>\n");
        exit(1);
    }
    
    
    f = fopen(imagename, "r");
    if (f == NULL) {
        fprintf(stderr, "Error opening file \n");
        exit(1);
    }
    
    // go to the beginning of file, read the first n bytes of the superblock into sb, check for errors
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
    
    // calculate start of the "root directory entries" section. Go there and read the first entry
    int dir_start = sb.block_size + (sb.fat_blocks * sb.block_size);
    fseek(f, dir_start, SEEK_SET);
    fread(&dir_entry, sizeof(dir_entry), 1, f);
   
    
    printf("ls%s output for %s \n\n", sb.magic, argv[2]);
    
    // look through each directory entry (in case entries aren't all together at the beginning)
    for (int iterations = 0; iterations < MAX_DIR_ENTRIES; iterations++) {
        
        // if directory is a file, convert contents endianness, convert modify time and print out all stats
        if (dir_entry.status == DIR_ENTRY_NORMALFILE){
            dir_entry.start_block = htonl(dir_entry.start_block);
            dir_entry.num_blocks = htonl(dir_entry.num_blocks);
            dir_entry.file_size = htonl(dir_entry.file_size);
            unpack_datetime(dir_entry.modify_time, &year, &month, &day, &hour, &minute, &second);
            printf("%8d %d-%s-%02d %02d:%02d:%02d %s\n", dir_entry.file_size, year, month_to_string(month), day, hour, minute, second, dir_entry.filename);
        }
        else {
            // pass
        }
        
        // get next directory entry
        fread(&dir_entry, sizeof(dir_entry), 1, f); 
    }
    
    printf("\n");
    fclose(f);

    return 0; 
}
