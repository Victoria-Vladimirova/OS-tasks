#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int sparse(int fd) {
	char byte;
	int ret;
	long long int counter = 0;

	while ((ret = read(0, &byte, 1)) > 0) {
		if (byte == 0) {
			counter++;
		} else {
			if (lseek(fd, counter, SEEK_CUR) == -1) {
				fprintf(stderr, "lseek error\n");
				exit(1);
			}
			counter = 0;

			if (write(fd, &byte, 1) == -1) {
				fprintf(stderr, "Cannot write file\n");
				exit(1);
			}
		}
	}

	if (byte == 0) { // файл закончился нулем
		if (lseek (fd, -1, SEEK_CUR) == -1) {
			fprintf(stderr, "lseek error\n");
			exit(1);
		}
		if (write(fd, &byte, 1) == -1) {
			fprintf(stderr, "Cannot write file\n");
			exit(1);
		}
	}

	if (ret == -1) {
		fprintf(stderr, "Cannot read file\n");
		exit(1);
	}

	return 0;
}

int main(int argc, char const *argv[]) {
	
	if (argc < 2) {
		fprintf(stderr, "Too few arguments\n");
		fprintf(stderr, "Usage: ./sparser <filename>\n");
		exit(1);
	}

	int fd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);

	if (fd == -1) {
		fprintf(stderr, "Cannot open file\n");
		exit(1);
	}

	sparse(fd);
	close(fd);

	return 0;
}