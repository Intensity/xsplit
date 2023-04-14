#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

const size_t BLOCK_SIZE = 65536;

typedef struct {
    dev_t device;
    ino_t inode;
} file_identifier;

void close_files (int * fds, int n) {
    for (int i = 0; i < n; ++ i) {
        close (fds [i]);
    }
}

bool check_duplicates (file_identifier * fis, int n) {
    for (int i = 0; i < n; ++i ) {
        for (int j = i + 1; j < n; ++ j) {
            if ((fis [i].device == fis [j].device) && (fis [i].inode == fis [j].inode)) {
                return true;
            }
        }
    }
    return false;
}

bool is_standard_output (int fd) {
    struct stat fd_stat, stdout_stat;
    if (fstat (fd, & fd_stat) != 0 || fstat (STDOUT_FILENO, & stdout_stat) != 0) {
        perror ("Error calling fstat");
        return false;
    }

    return (fd_stat.st_dev == stdout_stat.st_dev) && (fd_stat.st_ino == stdout_stat.st_ino);
}

int main(int argc, char * argv [ ]) {
    if (argc < 4) {
        fprintf (stderr, "Usage: %s input_file output_file1 output_file2 [...]\n", argv [0]);
        return 2;
    }

    int input_fd;
    if (strcmp (argv [1], "-") == 0) {
        input_fd = STDIN_FILENO;
    } else {
        input_fd = open (argv [1], O_RDONLY);
    }

    if (input_fd < 0) {
        perror ("Error opening input file");
        return 1;
    }

    int n = argc - 2;

    int * output_fds = calloc (n, sizeof (int));
    if (! output_fds) {
        perror ("Error allocating memory for output file descriptors");
        close (input_fd);
        return 1;
    }

    file_identifier * identifiers = calloc (n, sizeof (file_identifier));
    if (! identifiers) {
        perror ("Error allocating memory for file identifiers");
        close_files (output_fds, n);
        close (input_fd);
        return 1;
    }

    int stdout_used = 0;

    for (int i = 0; i < n; ++ i) {
        if (strcmp (argv [i + 2], "-") == 0) {
            output_fds [i] = STDOUT_FILENO;
            stdout_used ++;
        } else {
            output_fds [i] = open (argv [i + 2], O_WRONLY | O_APPEND | O_CREAT /* | O_BINARY */ , 0640);

            if (is_standard_output (output_fds [i]))
                stdout_used ++;
        }

        if (stdout_used > 1) {
            fprintf (stderr, "Error: standard output can only be used once\n");
            close_files (output_fds, i);
            close (input_fd);
            return 2;
        }

        if (output_fds [i] < 0) {
            perror ("Error opening output file");
            close_files (output_fds, i);
            close (input_fd);
            return 1;
        }

        struct stat file_stat;
        if (fstat (output_fds [i], & file_stat) != 0) {
            perror ("Error calling fstat");
            close_files (output_fds, n);
            close (input_fd);
            return 1;
        }

        identifiers [i].device = file_stat.st_dev;
        identifiers [i].inode = file_stat.st_ino;
    }

    if (check_duplicates (identifiers, n)) {
        fprintf (stderr, "Error: duplicate output files detected\n");
        close_files (output_fds, n);
        close (input_fd);
        return 1;
    }

    uint8_t * xor = calloc (2 * BLOCK_SIZE, sizeof (uint8_t));
    if (! xor) {
        perror ("Error allocating memory for xor and staging buffers");
        close_files (output_fds, n);
        close (input_fd);
        return 1;
    }

    uint8_t * staging = xor + BLOCK_SIZE;

    ssize_t read_len;
    ssize_t remaining_bytes;

    int urandom_fd = open ("/dev/urandom", O_RDONLY);
    if (urandom_fd < 0) {
        perror ("Error opening /dev/urandom");
        close_files (output_fds, n);
        close (input_fd);
        return 1;
    }

    while ((read_len = read (input_fd, xor, BLOCK_SIZE)) > 0) {
        remaining_bytes = read_len;

        while (remaining_bytes < BLOCK_SIZE) {
            ssize_t extra_read = read (input_fd, xor + remaining_bytes, BLOCK_SIZE - remaining_bytes);
            if (extra_read <= 0) break;
            remaining_bytes += extra_read;
        }

        for (int i = 0; i < n - 1; ++ i) {
            ssize_t urandom_read = read (urandom_fd, staging, BLOCK_SIZE);
            ssize_t remaining_urandom = urandom_read;

            while (remaining_urandom < BLOCK_SIZE) {
                ssize_t extra_urandom = read (urandom_fd, staging + remaining_urandom, BLOCK_SIZE - remaining_urandom);

                if (extra_urandom <= 0)
                    break;

                remaining_urandom += extra_urandom;
            }

            for (size_t j = 0; j < remaining_bytes; ++ j) {
                xor [j] ^= staging [j];
            }

            if (write (output_fds [i], staging, remaining_bytes) != remaining_bytes) {
                perror ("Error writing to output file");
                close (urandom_fd);
                close_files (output_fds, n);
                close (input_fd);
                free (xor);
                return 1;
            }
        }

        if (write (output_fds [n - 1], xor, remaining_bytes) != remaining_bytes) {
            perror ("Error writing to output file");
            close (urandom_fd);
            close_files (output_fds, n);
            close (input_fd);
            free (xor);
            return 1;
        }
    }

    if (read_len < 0) {
        perror ("Error reading input file");
    }

    close (urandom_fd);
    close_files (output_fds, n);
    close (input_fd);

    return read_len < 0 ? 1 : 0;
}
