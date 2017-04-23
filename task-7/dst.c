#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include "common.h"

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {

    if (argc < 4) {
        printf("usage: ./dst outfile size indescriptor [indescriptor ...]\n");
        return 1;
    }

    char *outfile = argv[1];
    FILE *out = fopen(outfile, "w");

    int filesize = atoi(argv[2]);

    int i, j;
    int handler_count = argc - 3;
    if (handler_count > MAX_HANDLERS) {
        handler_count = MAX_HANDLERS;
    }

    int indescriptors[handler_count];
    int block_counts[handler_count];

    for (i = 0; i < handler_count; i++) {
        indescriptors[i] = atoi(argv[i + 3]);
        block_counts[i] = 0;
    }

    int piece_size = filesize / handler_count;
    if (filesize % handler_count > 0) {
        piece_size++;
    }

    while (1) {

        fd_set set;
        FD_ZERO(&set);
        int fd_max = 0;
        for (j = 0; j < handler_count; j++) {
            FD_SET(indescriptors[j], &set);
            if (indescriptors[j] > fd_max) {
                fd_max = indescriptors[j];
            }
        }

        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int selval;
        if ((selval = select(fd_max + 1, &set, NULL, NULL, &tv)) < 0) {
            perror("Failed to select in dst");
            return 1;
        } else if (selval > 0) {
            for (j = 0; j < handler_count; j++) {
                if (FD_ISSET(indescriptors[j], &set)) {
                    char *buffer = (char *) malloc(BUFFER_SIZE);
                    int count = read(indescriptors[j], buffer, BUFFER_SIZE);
                    int file_position = j * piece_size + block_counts[j] * BUFFER_SIZE;
                    fseek(out, file_position, SEEK_SET);
                    fwrite(buffer, 1, count, out);
                    block_counts[j]++;
                    break;
                }
            }
        } else {
            break;
        }
    }
    fclose(out);

    return 0;
}