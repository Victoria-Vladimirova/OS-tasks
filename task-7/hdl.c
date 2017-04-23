#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <errno.h>
#include <string.h>

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {

    if (argc != 3) {
        printf("usage: ./hdl indescriptor outdescriptor\n");
        return 1;
    }

    int indescriptor = atoi(argv[1]);
    int outdescriptor = atoi(argv[2]);

    while (1) {
        fd_set set_in;
        FD_ZERO(&set_in);
        FD_SET(indescriptor, &set_in);

        struct timeval tv_in;
        tv_in.tv_sec = 1;
        tv_in.tv_usec = 0;

        int selval;
        if ((selval = select(indescriptor + 1, &set_in, NULL, NULL, &tv_in)) < 0) {
            perror("Failed to select in hdl for read");
            return 1;
        } else if (selval > 0) {
            int count;
            char buffer[BUFFER_SIZE];
            count = read(indescriptor, &buffer, BUFFER_SIZE);
            if (count > 0) {
                int i;
                for (i = 0; i < count; i++) {
                    if (buffer[i] >= 'a' && buffer[i] <= 'z') {
                        buffer[i] += 'A' - 'a';
                    }
                }
                fd_set set_out;
                FD_ZERO(&set_out);
                FD_SET(outdescriptor, &set_out);

                struct timeval tv_out;
                tv_out.tv_sec = 1;
                tv_out.tv_usec = 0;

                if ((selval = select(outdescriptor + 1, NULL, &set_out, NULL, &tv_out)) < 0) {
                    perror("Failed to select in hdl for write");
                    return 1;
                } else if (selval > 0) {
                    write(outdescriptor, &buffer, count);
                } else {
                    break;
                }
            }
        } else {
            break;
        }
    }
    close(indescriptor);
    close(outdescriptor);
    return 0;
}