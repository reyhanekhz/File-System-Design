#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    int32_t magic;
    int32_t file_system_version;
    int32_t files_count;

    int32_t last_allocated_offset;  
    int32_t free_list_head;       
} file_system_header;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
    char name[64];
    int32_t type;
    int32_t permission;
    int32_t size;
    int32_t data_offset;
    int32_t next; 
} file_metadata;
#pragma pack(pop)


typedef struct {
    int32_t metadata_index;
    int32_t pos;
    int is_open;
} file_handler;



#define MAX_FILES 1024
#define CREATE 1


// Load and save FS header
int read_fs_header(int file_descriptor, file_system_header *header);
int write_fs_header(int file_descriptor, const file_system_header *header);

// Metadata operations
int read_metadata(int file_descriptor, int index, file_metadata *meta);
int write_metadata(int file_descriptor, int index, const file_metadata *meta);

int find_file_by_name(int file_descriptor, const char *filename);
int find_free_metadata_slot(int file_descriptor);

// Open/close
file_handler open_file(int file_descriptor, const char *filename, int flags);
int close_file(file_handler *fh);

// Read / Write
int fs_read(int file_descriptor, file_handler *fh, int32_t pos, int32_t n, char *buffer);
int fs_write(int file_descriptor, file_handler *fh, int32_t pos, const char *buffer, int32_t n);

// File operations
int shrink_file(int file_descriptor, file_handler *fh, int32_t new_size);
int rm_file(int file_descriptor, file_handler *fh);

// Stats
int get_file_stats(int file_descriptor, file_handler *fh);
int get_fs_stats(int file_descriptor);

// Free List Structures
#pragma pack(push, 1)
typedef struct {
    int32_t start;
    int32_t size;
    int32_t next;   // index of next free block in linked list (TO MERGE)
} free_block;
#pragma pack(pop)

int init_free_list(int file_descriptor);
int find_free_block(int file_descriptor, int32_t size);
int allocate_space(int file_descriptor, int32_t size); // returns offset
void merge_free_list(int file_descriptor);
int free_space(int file_descriptor, int32_t start, int32_t size);

// Free list block read/write
int read_free_block(int file_descriptor, int index, free_block *block);
int write_free_block(int file_descriptor, int index, const free_block *block);
void print_free_list(int file_descriptor);
#define MAX_FREE_BLOCKS 1024


#endif
