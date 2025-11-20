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

void close_file(file_handler *fh) {
    fh->is_open = 0;
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

    // Allocate new space only if file has no data yet
    if (meta.data_offset == 0) {
        meta.data_offset = header.last_allocated_offset;
        header.last_allocated_offset += n;
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



int shrink_file(int file_descriptor, file_handler *fh, int32_t new_size) {
    if (!fh->is_open) return -1;

    file_metadata meta;
    read_metadata(file_descriptor, fh->metadata_index, &meta);

    // If new size is negative or is bigger than it's current size, invalid
    if (new_size < 0 || new_size > meta.size)
        return -1;

    meta.size = new_size;
    return write_metadata(file_descriptor, fh->metadata_index, &meta);
}


int rm_file(int file_descriptor, file_handler *fh) {
    if (!fh->is_open) return -1;

    // Zero the file's metadata
    file_metadata empty;
    memset(&empty, 0, sizeof(empty));
    write_metadata(file_descriptor, fh->metadata_index, &empty);

    // Decrement header's file count
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

int get_fs_stats(int file_descriptor) {
    file_system_header header;

    if (read_fs_header(file_descriptor, &header) != 0) {
        printf("Error reading header.\n");
        return -1;
    }

    // Get total file size 
    off_t fs_size = lseek(file_descriptor, 0, SEEK_END);

    int32_t header_size = sizeof(file_system_header);
    int32_t metadata_total = sizeof(file_metadata) * MAX_FILES;

    int32_t data_start = header_size + metadata_total;
    int32_t data_used = header.last_allocated_offset - data_start;

    int32_t used_space = header_size + metadata_total + data_used;
    int32_t free_space = fs_size - used_space;

    printf("Filesystem Stats\n");
    printf("Number of files: %d\n", header.files_count);
    printf("Used space: %d bytes\n", used_space);
    printf("Free space: %d bytes\n", free_space);

    return 0;
}
