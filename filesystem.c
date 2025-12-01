#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "filesystem.h"


int read_fs_header(int file_descriptor, file_system_header *header) {
    if (lseek(file_descriptor, 0, SEEK_SET) == -1) return -1;
    if (read(file_descriptor, header, sizeof(*header)) != sizeof(*header)) return -1;
    return 0;
}


int write_fs_header(int file_descriptor, const file_system_header *header) {
    if (lseek(file_descriptor, 0, SEEK_SET) == -1) return -1;
    if (write(file_descriptor, header, sizeof(*header)) != sizeof(*header)) return -1;
    return 0;
}

int read_metadata(int file_descriptor, int index, file_metadata *meta) {
    off_t offset = sizeof(file_system_header) + index * sizeof(file_metadata);
    if (lseek(file_descriptor, offset, SEEK_SET) == -1) return -1;
    if (read(file_descriptor, meta, sizeof(*meta)) != sizeof(*meta)) return -1;
    return 0;
}

int write_metadata(int file_descriptor, int index, const file_metadata *meta) {
    off_t offset = sizeof(file_system_header) + index * sizeof(file_metadata);
    if (lseek(file_descriptor, offset, SEEK_SET) == -1) return -1;
    if (write(file_descriptor, meta, sizeof(*meta)) != sizeof(*meta)) return -1;
    return 0;
}

int find_file_by_name(int file_descriptor, const char *filename) {
    file_metadata meta;
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        // Read each file's metadata
        if (read_metadata(file_descriptor, i, &meta) != 0) continue;
        if (meta.name[0] == 0) continue;
        // Find the name we're looking for and return it's index
        if (strcmp(meta.name, filename) == 0) return i;
    }
    return -1;
}

int find_free_metadata_slot(int file_descriptor) {
    file_metadata meta;
    // Find last empty file
    int i;
    for (i = 0; i < MAX_FILES; i++) {
        if (read_metadata(file_descriptor, i, &meta) != 0) continue;
        if (meta.name[0] == 0) return i;
    }
    return -1;
}


file_handler open_file(int file_descriptor, const char *filename, int flags) {
    file_handler fh = {
        .metadata_index = -1,
        .pos = 0,
        .is_open = 0
    };

    int index = find_file_by_name(file_descriptor, filename);

    // If file already exists, just is_open
    if (index != -1) {
        fh.metadata_index = index;
        fh.pos = 0;
        fh.is_open = 1;
        return fh;
    }

    // If not exist and CREATE flag not set, give error
    if (!(flags & CREATE)) {
        printf("Error: file '%s' does not exist and CREATE not set.\n", filename);
        return fh;
    }

    // Create new file
    int free_index = find_free_metadata_slot(file_descriptor);
    if (free_index == -1) {
        printf("Error: no free metadata available.\n");
        return fh;
    }

    file_metadata meta;
    // Zero the meta first
    memset(&meta, 0, sizeof(meta));
    // Copy the filename you want to create in the meta's name field
    strncpy(meta.name, filename, 63);
    meta.type = 1;
    meta.permission = 0;
    meta.size = 0;
    meta.data_offset = 0;
    meta.next = -1;

    write_metadata(file_descriptor, free_index, &meta);

    // Update FS header's file count
    file_system_header header;
    read_fs_header(file_descriptor, &header);
    header.files_count++;
    write_fs_header(file_descriptor, &header);

    fh.metadata_index = free_index;
    fh.pos = 0;
    fh.is_open = 1;
    return fh;
}
int close_file(file_handler *fh) {
    if (!fh->is_open)
        return -1;

    fh->is_open = 0;

    return 0;
}


int fs_read(int file_descriptor, file_handler *fh, int32_t pos, int32_t n, char *buffer) {
    // If file is not is_open, you can't read it
    if (!fh->is_open) return -1;

    file_metadata meta;
    read_metadata(file_descriptor, fh->metadata_index, &meta);

    // If pos is out of file's size,invalid
    if (pos >= meta.size) return 0;

    // If the read data is out of file's size, read until the end of file
    if (pos + n > meta.size)
        n = meta.size - pos;

    off_t offset = meta.data_offset + pos;

    if (lseek(file_descriptor, offset, SEEK_SET) == -1) return -1;
    return read(file_descriptor, buffer, n);
}

 

int fs_write(int file_descriptor, file_handler *fh, int32_t pos, const char *buffer, int32_t n) {
    if (!fh->is_open) return -1;

    file_system_header header;
    read_fs_header(file_descriptor, &header);

    file_metadata meta;
    read_metadata(file_descriptor, fh->metadata_index, &meta);

    if (meta.data_offset == 0) {
        int off = allocate_space(file_descriptor, n);
        if (off == -1) {
            printf("No free space!\n");
            return -1;
        }
        meta.data_offset = off;
    }


    // Extend file size if needed
    if (pos + n > meta.size)
        meta.size = pos + n;

    // Write new metadata + new header
    write_metadata(file_descriptor, fh->metadata_index, &meta);
    write_fs_header(file_descriptor, &header);

    // Write data
    off_t offset = meta.data_offset + pos;
    if (lseek(file_descriptor, offset, SEEK_SET) == -1) return -1;

    return write(file_descriptor, buffer, n);
}


int shrink_file(int fd, file_handler *fh, int32_t new_size) {
    if (!fh->is_open) return -1;

    file_metadata meta;
    read_metadata(fd, fh->metadata_index, &meta);

    if (new_size < 0 || new_size > meta.size) return -1;

    if (new_size < meta.size) {
        int freed_start = meta.data_offset + new_size;
        int freed_size  = meta.size - new_size;

        free_space(fd, freed_start, freed_size);
    }

    meta.size = new_size;
    return write_metadata(fd, fh->metadata_index, &meta);
}



int rm_file(int file_descriptor, file_handler *fh) {
    if (!fh->is_open) return -1;

    file_metadata meta;
    read_metadata(file_descriptor, fh->metadata_index, &meta);

    // free the data block
    if (meta.data_offset != 0 && meta.size > 0)
        free_space(file_descriptor, meta.data_offset, meta.size);

    // Zero metadata
    file_metadata empty;
    memset(&empty, 0, sizeof(empty));
    write_metadata(file_descriptor, fh->metadata_index, &empty);

    // Update FS header
    file_system_header header;
    read_fs_header(file_descriptor, &header);
    header.files_count--;
    write_fs_header(file_descriptor, &header);

    fh->is_open = 0;
    return 0;
}



int get_file_stats(int file_descriptor, file_handler *fh) {
    if (!fh->is_open) return -1;

    file_metadata meta;
    read_metadata(file_descriptor, fh->metadata_index, &meta);

    printf("File Stats:\n");
    printf("Name: %s\n", meta.name);
    printf("Size: %d\n", meta.size);
    printf("Data Offset: %d\n", meta.data_offset);

    return 0;
}
int get_fs_stats(int fd) {
    file_system_header header;
    read_fs_header(fd, &header);

    off_t total_size = lseek(fd, 0, SEEK_END);

    // 1. Compute free space by summing free blocks
    int32_t free_space = 0;
    free_block blk;

    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        if (read_free_block(fd, i, &blk) != 0) continue;
        free_space += blk.size;
    }

    int32_t used_space = total_size - free_space;

    printf("Filesystem Stats:\n");
    printf("Number of files: %d\n", header.files_count);
    printf("Used space: %d bytes\n", used_space);
    printf("Free space: %d bytes\n", free_space);

    return 0;
}


int free_block_offset(int index) {
    return sizeof(file_system_header)
         + sizeof(file_metadata) * MAX_FILES
         + sizeof(free_block) * index;
}


int read_free_block(int file_descriptor, int index, free_block *block) {
    off_t off = free_block_offset(index);
    lseek(file_descriptor, off, SEEK_SET);
    return read(file_descriptor, block, sizeof(*block)) == sizeof(*block) ? 0 : -1;
}

int write_free_block(int file_descriptor, int index, const free_block *block) {
    off_t off = free_block_offset(index);
    lseek(file_descriptor, off, SEEK_SET);
    return write(file_descriptor, block, sizeof(*block)) == sizeof(*block) ? 0 : -1;
}

int init_free_list(int file_descriptor) {
    file_system_header header;
    read_fs_header(file_descriptor, &header);

    // Compute total FS size dynamically
    int32_t fs_size = lseek(file_descriptor, 0, SEEK_END);

    // Initialize head of linked list
    header.free_list_head = 0;

    free_block blk;
    blk.start = header.last_allocated_offset;  // data region start
    blk.size  = fs_size - blk.start;           // free space = everything after metadata/free-blocks
    blk.next  = -1;

    write_free_block(file_descriptor, 0, &blk);
    write_fs_header(file_descriptor, &header);

    return 0;
}


int allocate_space(int fd, int32_t size) {
    file_system_header header;
    read_fs_header(fd, &header);

    int prev = -1;
    int cur  = header.free_list_head;

    free_block blk;

    while (cur != -1) {
        if (read_free_block(fd, cur, &blk) != 0)
            return -1;

        if (blk.size >= size) {

            int alloc_start = blk.start;

            blk.start += size;
            blk.size  -= size;

            if (blk.size == 0) {
                if (prev == -1) header.free_list_head = blk.next;
                else {
                    free_block p;
                    read_free_block(fd, prev, &p);
                    p.next = blk.next;
                    write_free_block(fd, prev, &p);
                }
            } else {
                write_free_block(fd, cur, &blk);
            }

            write_fs_header(fd, &header);
            return alloc_start;
        }

        prev = cur;
        cur = blk.next;
    }

    return -1;
}


int find_free_block_slot(int fd) {
    free_block blk;
    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        read_free_block(fd, i, &blk);
        if (blk.size == 0) return i;   // empty block
    }
    return -1;
}
int free_space(int file_descriptor, int32_t start, int32_t size) {

    file_system_header header;
    read_fs_header(file_descriptor, &header);

    int slot = find_free_block_slot(file_descriptor);
    if (slot == -1) {
        printf("Free list FULL! cannot free space.\n");
        return -1;
    }

    free_block new_block;
    new_block.start = start;
    new_block.size  = size;
    new_block.next  = header.free_list_head;

    write_free_block(file_descriptor, slot, &new_block);

    header.free_list_head = slot;
    write_fs_header(file_descriptor, &header);

    merge_free_list(file_descriptor);

    return 0;
}

void merge_free_list(int file_descriptor) {
    free_block a, b;

    for (int i = 0; i < MAX_FREE_BLOCKS; i++) {
        if (read_free_block(file_descriptor, i, &a) != 0) continue;
        if (a.size == 0) continue;

        for (int j = 0; j < MAX_FREE_BLOCKS; j++) {
            if (i == j) continue;
            if (read_free_block(file_descriptor, j, &b) != 0) continue;
            if (b.size == 0) continue;

            // a end == b start ---> merge
            if (a.start + a.size == b.start) {
                a.size += b.size;
                b.size = 0;

                write_free_block(file_descriptor, i, &a);
                write_free_block(file_descriptor, j, &b);
            }
        }
    }
}

