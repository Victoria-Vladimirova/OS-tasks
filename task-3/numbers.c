#include <stdio.h>
#include <stdlib.h>

#define MAX_LEN 1000000

float numbers[MAX_LEN];
long long numbersLength = 0;

int getNumbers(char * fileName) {

	FILE * ptrFile = fopen(fileName, "r");
 
	if (ptrFile == NULL) {
		printf("Cannot open file %s\n", fileName);
	    return -1;
    }
	
    int result = 0;
    int count;
    while ((count = fscanf(ptrFile, "%f", &numbers[numbersLength])) != EOF) {
    	if (count > 0) {
    		if (numbersLength == MAX_LEN) {
    			printf("Maximum number count is %d\n", MAX_LEN);
    			result = 1;
    			break;
    		}

    		numbersLength++;
    	} else {
    		getc(ptrFile);
    	}
    }

	fclose(ptrFile);

	return result;
}

int compare(const void * a, const void * b) {
    if (*((float*)a) > *((float*)b)) return 1;
    if (*((float*)a) < *((float*)b)) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
	
	if (argc < 3) {
		fprintf(stderr, "Too few arguments\n");
		fprintf(stderr, "Usage: ./numbers [<input_file> ...] <output_file> \n");
		exit(1);
	}

	int i;

	for (i = 1; i < argc - 1; i++) {
		if (getNumbers(argv[i]) == 1) {
			break;
		}
	}

	qsort(numbers, numbersLength, sizeof(float), compare);

    FILE * ptrFile = fopen(argv[argc - 1], "w");
	
	if (ptrFile == NULL) {
	    printf("Cannot open file %s\n", argv[argc - 1]);
	    exit(1);
    }
    
    int result = 0;
    int res = 0;
	for (i = 0; i < numbersLength; i++) {
		if (numbers[i] == (int) numbers[i]) {
    		res = fprintf(ptrFile, "%d\n", (int) numbers[i]);
    	} else {
    		res = fprintf(ptrFile, "%f\n", numbers[i]);
    	}

		if (res < 0) {
			printf("Cannot write file %s\n", argv[argc - 1]);
			result = 1;
			break;
		}
	}

	fclose(ptrFile);

	return result;
}
