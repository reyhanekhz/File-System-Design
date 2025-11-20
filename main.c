#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "filesystem.h"

int initialize_filesystem(const char *path, int32_t size_bytes) {
    int file_descriptor = open(path, O_RDWR);

    if (file_descriptor != -1) {
        // filesystem.db exists so verify header
        file_system_header header;

        if (read(file_descriptor, &header, sizeof(header)) != sizeof(header)) {
            printf("Error: filesys.db is unavailable. Reinitializing...\n");
        } else if (header.magic == 0xDEADBEEF && header.file_system_version == 1) {
            printf("Filesystem loaded.\n");
            return file_descriptor;
        }

        // If unavailable then reinit
        close(file_descriptor);
    }

    // Create new filesystem
    printf("filesys.db not found — creating new filesystem...\n");

    file_descriptor = open(path, O_RDWR | O_CREAT, 0644);
    if (file_descriptor == -1) {
        perror("open");
        return -1;
    }

    if (ftruncate(file_descriptor, size_bytes) != 0) {
        perror("ftruncate");
        close(file_descriptor);
        return -1;
    }

    // Build header
    file_system_header header;
    header.magic = 0xDEADBEEF;
    header.file_system_version = 1;
    header.files_count = 0;

    int32_t header_size = sizeof(file_system_header);
    int32_t metadata_size = sizeof(file_metadata);

    header.last_allocated_offset = header_size + metadata_size * MAX_FILES;

    // Write header
    lseek(file_descriptor, 0, SEEK_SET);
    write(file_descriptor, &header, sizeof(header));

    // Zero metadata
    char zero[4096] = {0};
    lseek(file_descriptor, header_size, SEEK_SET);

    size_t total_meta = metadata_size * MAX_FILES;
    while (total_meta > 0) {
        size_t chunk = total_meta > 4096 ? 4096 : total_meta;
        write(file_descriptor, zero, chunk);
        total_meta -= chunk;
    }

    fsync(file_descriptor);

    printf("Filesystem created successfully.\n");
    return file_descriptor;
}


// MAIN SHELL
int main() {
    int file_descriptor = initialize_filesystem("filesys.db", 1024 * 1024); // 1MB
    if (file_descriptor == -1) return 1;

    char command[256];
    char arg1[128], arg2[128];
    int pos, n;

    printf("\nFileSystem Shell Ready.\n");

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(command, sizeof(command), stdin))
            break;

        // OPEN
        if (sscanf(command, "open %s %s", arg1, arg2) == 2) {
            int flags = (strcmp(arg2, "CREATE") == 0) ? CREATE : 0;

            file_handler fh = open_file(file_descriptor, arg1, flags);
            if (fh.is_open)
                printf("Opened file %s.\n", arg1);
            else
                printf("Failed to open %s.\n", arg1);

            continue;
        }

        // READ
        if (sscanf(command, "read %s %d %d", arg1, &pos, &n) == 3) {
            int idx = find_file_by_name(file_descriptor, arg1);

            if (idx == -1) {
                printf("File not found.\n");
                continue;
            }

            file_handler fh = { idx, 0, 1 };

            char buf[4096];
            int r = fs_read(file_descriptor, &fh, pos, n, buf);

            if (r > 0) {
                buf[r] = '\0';
                printf("Read: %s\n", buf);
            } else {
                printf("Nothing read.\n");
            }

            continue;
        }

        // WRITE
        if (sscanf(command, "write %s %d %s", arg1, &pos, arg2) == 3) {
            int idx = find_file_by_name(file_descriptor, arg1);

            if (idx == -1) {
                printf("File not found.\n");
                continue;
            }

            file_handler fh = { idx, 0, 1 };
            int w = fs_write(file_descriptor, &fh, pos, arg2, strlen(arg2));
            printf("Wrote %d bytes.\n", w);
            continue;
        }

        // RM
        if (sscanf(command, "rm %s", arg1) == 1) {
            int idx = find_file_by_name(file_descriptor, arg1);

            if (idx == -1) {
                printf("File not found.\n");
                continue;
            }

            file_handler fh = { idx, 0, 1 };
            rm_file(file_descriptor, &fh);

            printf("Removed %s.\n", arg1);
            continue;
        }

        // FILE STATS
        if (sscanf(command, "stat %s", arg1) == 1) {
            int idx = find_file_by_name(file_descriptor, arg1);

            if (idx == -1) {
                printf("File not found.\n");
                continue;
            }

            file_handler fh = { idx, 0, 1 };
            get_file_stats(file_descriptor, &fh);
            continue;
        }

        // FS STATS
        if (strcmp(command, "fsstat\n") == 0) {
            get_fs_stats(file_descriptor);
            continue;
        }

        // EXIT
        if (strcmp(command, "exit\n") == 0)
            break;

        printf("Unknown command.\n");
    }

    close(file_descriptor);
    return 0;
}
