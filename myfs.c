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
} file_system_header;
#pragma pack(push)


#pragma pack(push, 1)
typedef struct {
    char name[64];
    int32_t type;
    int32_t permission;

} file_metadata;
#pragma pack(pop)


int32_t initialize_filesystem(const char *file_path, int32_t total_size_bytes) {
    const int32_t file_system_header_size = sizeof(file_system_header);
    const int32_t file_metadata_size = sizeof(file_metadata);
    const int32_t max_number_of_files = 1024;

    int file_descriptor = open(file_path, O_RDWR | O_CREAT, 0644);
    if (file_descriptor == -1) {
        perror("open");
        return -1;
    }

    if (ftruncate (file_descriptor, ))


    
}


int main(void) {
    const char *file_path = "filesys.db";
    int32_t total_size_bytes = 1024 * 1024;



    return 0;
}