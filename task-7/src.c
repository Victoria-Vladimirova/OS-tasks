#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <limits.h>
#include "common.h"

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {

    if (argc < 3) {
        printf("usage: ./src infile outdescriptor [outdescriptor ...]\n");
        return 1;
    }

    char *infile = argv[1];

    int i;
    int handler_count = argc - 2;
    if (handler_count > MAX_HANDLERS) {
        handler_count = MAX_HANDLERS;
    }
    int outdescriptors[handler_count];
    for (i = 0; i < handler_count; i++) {
        outdescriptors[i] = atoi(argv[i + 2]);
    }

    FILE *in = fopen(infile, "r");
    if (in == NULL) {
        perror("Error opening input file");
        return 1;
    }

    // определяем размер файла
    fseek(in, 0, SEEK_END);
    int filesize = ftell(in);
    rewind(in);

    int piece_size = filesize / handler_count;
    if (filesize % handler_count > 0) {
        piece_size++;
    }

    char buf[BUFFER_SIZE];
    for (i = 0; i < handler_count; i++) {
        int piece_size_left = piece_size;
        while (piece_size_left > 0) {
            int toread = BUFFER_SIZE;
            if (piece_size_left < BUFFER_SIZE) {
                toread = piece_size_left;
            }
            int count = fread(buf, 1, toread, in);
            if (count < toread) {
                // дочитали до конца файла
                piece_size_left = 0;
            }
            piece_size_left -= count;
            if (count <= 0) {
                break;
            }

            fd_set set;
            FD_ZERO(&set);
            FD_SET(outdescriptors[i], &set);

            struct timeval tv;
            tv.tv_sec = 1;
            tv.tv_usec = 0;

            int selval;
            if ((selval = select(outdescriptors[i] + 1, NULL, &set, NULL, &tv)) < 0) {
                perror("Failed to select in src");
                return 1;
            } else if (selval > 0) {
                int j;
                for (j = 0; j < handler_count; j++) {
                    if (FD_ISSET(outdescriptors[j], &set)) {
                        write(outdescriptors[j], &buf, count);
                        break;
                    }
                }
            } else {
                break;
            }
        }
    }

    fclose(in);
    return 0;
}