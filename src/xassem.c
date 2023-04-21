#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

const size_t BLOCK_SIZE = 65536;

bool is_standard_input (int fd) {
    struct stat fd_stat, stdin_stat;
    if (   fstat (fd, &fd_stat) != 0
        || fstat (STDIN_FILENO, & stdin_stat) != 0) {

        perror ("Error calling fstat");
        return false;
    }

    return    (fd_stat.st_dev == stdin_stat.st_dev)
           && (fd_stat.st_ino == stdin_stat.st_ino);
}

void free_resources (int * input_fds, int n, int output_fd, unsigned char * buffer) {
    for (int j = 0; j < n; ++ j) {
        close (input_fds [j]);
    }

    free (input_fds);
    memset (buffer, 0, 2 * BLOCK_SIZE);
    free (buffer);
    close (output_fd);
}

int main (int argc, char * argv [ ]) {
    if (argc < 4) {
        fprintf (stderr, "Usage: %s output_file input_file1 input_file2 [...]\n", argv [0]);
        return 2;
    }

    int output_fd;
    if (strcmp (argv [1], "-") == 0) {
        output_fd = STDOUT_FILENO;
    } else {
        output_fd = open (argv [1], O_WRONLY | O_CREAT | O_APPEND, 0640);
        if (output_fd < 0) {
            perror ("Error opening output file");
            return 1;
        }
    }

    int n = argc - 2;
    int * input_fds = calloc (n, sizeof (int));
    if (! input_fds) {
        perror ("Error allocating memory for input file descriptors");
        close (output_fd);
        return 1;
    }

    int stdin_used = 0;
    for (int i = 0; i < n; ++ i) {
        if (strcmp (argv [i + 2], "-") == 0) {
            input_fds [i] = STDIN_FILENO;
            stdin_used ++;
        } else {
            input_fds [i] = open (argv [i + 2], O_RDONLY);
            if (input_fds [i] < 0) {
                perror ("Error opening input file");
                for (int j = 0; j < i; ++j) {
                    close (input_fds [j]);
                }
                free (input_fds);
                close (output_fd);
                return 1;
            }

            if (is_standard_input (input_fds [i]))
                stdin_used ++;
        }

        if (stdin_used > 1) {
            fprintf (stderr, "Error: standard input can only be used once\n");
            free (input_fds);
            close (output_fd);
            return 2;
        }
    }

    unsigned char * buffer = calloc (2 * BLOCK_SIZE, sizeof (unsigned char));
    if (! buffer) {
        perror ("Error allocating memory for buffer");
        for (int i = 0; i < n; ++ i) {
            close (input_fds [i]);
        }
        free (input_fds);
        close (output_fd);
        return 1;
    }

    ssize_t bytes_read = 0;
    unsigned char * stage = buffer + BLOCK_SIZE;

    while (true) {
        bytes_read = 0;
        ssize_t remaining_bytes = BLOCK_SIZE;

        while (remaining_bytes > 0) {
            ssize_t extra_read = read (input_fds [0], buffer + bytes_read, remaining_bytes);

            if (extra_read < 0 || errno != 0) {
                if (errno != 0) {
                    perror ("Error reading input file");
                } else {
                    fprintf (stderr, "Error: read () returned a negative value but errno was not set\n");
                }

                break;
            } else if (extra_read == 0) {
                break;
            }

            bytes_read += extra_read;
            remaining_bytes -= extra_read;
        }

        if (bytes_read == 0) {
            break;
        }

        for (int i = 1; i < n; ++ i) {
            ssize_t input_bytes_read = 0;
            remaining_bytes = bytes_read;

            while (remaining_bytes > 0) {
                ssize_t extra_read = read (input_fds [i], stage + input_bytes_read, remaining_bytes);

                if (extra_read < 0 || errno != 0) {
                    if (errno != 0) {
                        perror ("Error reading input file");
                    } else {
                        fprintf (stderr, "Error: read () returned a negative value but errno was not set\n");
                    }

                    free_resources (input_fds, n, output_fd, buffer);
                    return 1;
                } else if (extra_read == 0) {
                    break;
                }

                input_bytes_read += extra_read;
                remaining_bytes -= extra_read;
            }

            if (input_bytes_read != bytes_read) {
                fprintf (stderr, "Error: input files have different lengths\n");
                free_resources (input_fds, n, output_fd, buffer);
                return 3;
            }

            for (ssize_t j = 0; j < bytes_read; ++ j) {
                buffer [j] ^= stage [j];
            }
        }

        ssize_t bytes_written = 0;
        remaining_bytes = bytes_read;

        while (remaining_bytes > 0) {
            ssize_t written = write (output_fd, buffer + bytes_written, remaining_bytes);

            if (written < 0 || errno != 0) {
                if (errno != 0) {
                    perror ("Error writing to output file");
                } else {
                    fprintf (stderr, "Error: write () returned a negative value but errno was not set\n");
                }

                free_resources (input_fds, n, output_fd, buffer);
                return 1;
            }

            bytes_written += written;
            remaining_bytes -= written;
        }

        memset (buffer, 0, 2 * BLOCK_SIZE);
    }

    if (bytes_read < 0) { perror ("Error reading from input file"); }

    for (int i = 0; i < n; ++ i) {
        if (read (input_fds [i], buffer, 1) != 0) {
            fprintf (stderr, "Error: input files have different lengths\n");
            free_resources (input_fds, n, output_fd, buffer);
            return 3;
        }

        if (errno != 0) {
            perror ("Error when checking length consistency");
        }
    }

    for (int i = 0; i < n; ++ i) { close (input_fds [i]); }

    free (input_fds);
    free (buffer);
    buffer = (unsigned char *) (NULL);
    close (output_fd);

    return 0;
}
