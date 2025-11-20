#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>


#pragma pack(push, 1)
typedef struct {
    int32_t magic;
    int32_t file_system_version;
    int32_t files_count;
    int32_t last_allocated_offset;
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


int32_t initialize_filesystem(const char *file_path, int32_t total_size_bytes) {
    // Create file with size 0 bytes first, with READ/WRITE OR CEATE flags
    // 644 sets the permissions
    int file_descriptor = open(file_path, O_RDWR | O_CREAT, 0644);
    // File Descriptor returns -1 if an error occurs
    if (file_descriptor == -1) {
        perror("open error");
        return -1;
    }

    // Now set size of disk
    if (ftruncate (file_descriptor, total_size_bytes) != 0) {
        perror("size set error");
        close(file_descriptor);
        return -1;
    }

    // Initialize file-system header in disk
    file_system_header header;
    header.magic = 0xDEADBEEF;
    header.file_system_version = 1;
    header.files_count = 0;

    const int32_t file_system_header_size = sizeof(file_system_header);
    const int32_t file_metadata_size = sizeof(file_metadata);
    const int32_t max_number_of_files = 1024;
    header.last_allocated_offset = file_system_header_size + file_metadata_size * max_number_of_files;

    // Move pointer to first byte
    if (lseek(file_descriptor, 0, SEEK_SET) == -1) {\
        perror("pointer error");
        close(file_descriptor);
        return -1;
    }

    // Write file-system's header to the beginning of disk
    if (write(file_descriptor, &header, sizeof(header)) != sizeof(header)) {
        perror("write error");
        close(file_descriptor);
        return -1;
    }

    // Place of Metadatas
    off_t metadata_start = file_system_header_size;
    size_t file_system_metadata_size = file_metadata_size * max_number_of_files;

    // Efficiently write zeros
    const size_t CHUNK = 4096;
    char zero_buf[CHUNK];
    memset(zero_buf, 0, CHUNK);

    // Move pointer to start of Metadata (after Header)
    if (lseek(file_descriptor, metadata_start, SEEK_SET) == -1) {
        perror("metadata pointer error");
        close(file_descriptor);
        return -1;
    }

    // Zero every 4KB, using the 4KB zero buffer
    size_t metadata_remaining = file_system_metadata_size;
    while (metadata_remaining > 0) {
        size_t w;
        if (metadata_remaining < CHUNK) {
            w = metadata_remaining;
        }
        else {
            w = CHUNK;
        }

        if (write(file_descriptor, zero_buf, w) != (ssize_t)w) {
            perror("Write zero error");
            close (file_descriptor);
            return -1;
        }

        metadata_remaining -= w;
    }

    fsync(file_descriptor);
    close(file_descriptor);
    return 0;
}


int main(void) {
    const char *file_path = "filesys.db";
    int32_t total_size_bytes = 1024 * 1024;

    if (initialize_filesystem(file_path, total_size_bytes) != 0){
        perror("Initialization");
        return 1;
    }

    printf("File-system initialization done successfully");
    return 0;

}