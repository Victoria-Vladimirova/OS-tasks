#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_ROWS 1000
#define MAX_USER_LENGTH 50
#define MAX_PASSWORD_LENGTH 50


char * getLockName(char * fileName, char * extension) {
    char * str = (char *)malloc(strlen(fileName) + strlen(extension) + 1);
    strcpy(str, fileName);
    strcat(str, extension);
    return str;
}

int lockLockFile(char * fileName) {
    if (!access(fileName, 0)) {
        return -1;
    }
    FILE * ptrFile = fopen(fileName, "w");
    fprintf(ptrFile, "%d", getpid());
    fclose(ptrFile);
    return 0;
}

void clearLockLock(char * fileName) {
    remove(fileName);
}

int lockFile(char * fileName, char * mode, char * user) {
    char * lockFileName = getLockName(fileName, ".lck\0");
    char * lockLockFileName = getLockName(fileName, ".lck.lck\0");

    // ждем, пока не сможем заблокировать .lck файл
    while (lockLockFile(lockLockFileName) != 0) {
        sleep(5);
    }

    sleep(20);

    FILE * ptrFile = fopen(lockFileName, "a+");
    // помещаем указатель в начало, чтобы посмотреть предыдущие записи
    fseek(ptrFile, 0, SEEK_SET);

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int isRecordExist = 0;
    char * token;

    while ((read = getline(&line, &len, ptrFile)) != -1) {
        // получаем последнее слово = user
        token = strtok(line, " \n");
        token = strtok(NULL, " \n");
        token = strtok(NULL, " \n");

        if (strcmp(token, user) == 0) {
            isRecordExist = 1;
            break;
        }
    }

    if (!isRecordExist) {
        // пишем в конец, если нужный кусок никто не заблокировал
        fseek(ptrFile, 0, SEEK_END);
        fprintf(ptrFile, "%d %s %s\n", getpid(), mode, user);
    }

    fclose(ptrFile);
    clearLockLock(lockLockFileName);

    return isRecordExist;
}

void clearLock(char * fileName, char * user) {
    char * lockFileName = getLockName(fileName, ".lck\0");
    char * lockLockFileName = getLockName(fileName, ".lck.lck\0");

    // ждем, пока не сможем заблокировать .lck файл
    while (lockLockFile(lockLockFileName) != 0) {
        sleep(5);
    }

    FILE * ptrFile = fopen(lockFileName, "r");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char * token;
    int i = 0;
    char records [50][100];

    while ((read = getline(&line, &len, ptrFile)) != -1) {
        char * dst = (char *) malloc(len);
        strcpy(dst, line);

        // получаем последнее слово = user
        token = strtok(dst, " \n");
        token = strtok(NULL, " \n");
        token = strtok(NULL, " \n");

        if (strcmp(token, user) != 0) {
            strcpy(records[i], line);
            i++;
        }
    }

    fclose(ptrFile);

    if (i == 0) {
        // если в файле после удаления строки не останется записей, то удаляем его
        remove(lockFileName);
    } else {
        ptrFile = fopen(lockFileName, "w");

        int j;
        for (j = 0; j < i; j++) {
            fprintf(ptrFile, "%s", records[j]);
        }
        fclose(ptrFile);
    }

    clearLockLock(lockLockFileName);
}

void changePassword(char * fileName, char * user, char * password) {
    // ждем, пока не сможем заблокировать файл
    while (lockFile(fileName, "write", user) != 0) {
        sleep(5);
    }

    sleep(20);

    FILE * ptrFile = fopen(fileName, "r");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char * token;
    int i = 0;
    int isUserExist = 0;

    char users [MAX_ROWS][MAX_USER_LENGTH];
    char passwords [MAX_ROWS][MAX_PASSWORD_LENGTH];

    while ((read = getline(&line, &len, ptrFile)) != -1 && i != MAX_ROWS) {
        token = strtok(line, " \n"); // user
        strcpy(users[i], token);

        if (strcmp(users[i], user) == 0) {
            // меняем пароль
            strcpy(passwords[i], password);
            isUserExist = 1;
        } else { 
            // оставляем тот же пароль
            token = strtok(NULL, " \n"); // password
            strcpy(passwords[i], token);
        }
        i++;
    }

    fclose(ptrFile);

    // допишем в конец, если такого пользователя еще не было
    if (!isUserExist && i < MAX_ROWS) {
        strcpy(users[i], user);
        strcpy(passwords[i], password);
        i++;
    }

    // запись изменений в файл
    ptrFile = fopen(fileName, "w");
    
    int j;
    for (j = 0; j < i; j++) {
        fprintf(ptrFile, "%s %s\n", users[j], passwords[j]);
    }

    fclose(ptrFile);
    clearLock(fileName, user);
}

void getPassword(char * fileName, char * user) {
    // ждем, пока не сможем заблокировать файл
    while (lockFile(fileName, "read", user) != 0) {
        sleep(5);
    }

    sleep(20);

    FILE * ptrFile = fopen(fileName, "r");

    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    char * token;

    while ((read = getline(&line, &len, ptrFile)) != -1) {
        token = strtok(line, " \n"); // user
        if (strcmp(token, user) == 0) {
            token = strtok(NULL, " \n"); // password
            printf("Password for user '%s' is '%s'\n", user, token);
            break;
        }
    }

    fclose(ptrFile);
    clearLock(fileName, user);
}

int main(int argc, char *argv[]) {
    if (argc == 3) {
        getPassword(argv[1], argv[2]);
    }

    if (argc == 4) {
        changePassword(argv[1], argv[2], argv[3]);
    }

    return 0;
}