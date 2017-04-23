#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "common.h"

int main(int argc, char **argv) {

    if (argc != 4) {
        printf("Usage: ./launcher <source_file> <dest_file> <handler_count>\n");
        return 1;
    }

    char *source_file = argv[1];
    char *dest_file = argv[2];
    int handler_count = atoi(argv[3]);

    if (handler_count > MAX_HANDLERS) {
        printf("Too many handlers, MAX_HANDLERS = %d will be used\n", MAX_HANDLERS);
        handler_count = MAX_HANDLERS;
    }

    // цикл создания каналов
    int pf_src_to_hdl[handler_count][2], pf_hdl_to_dst[handler_count][2];
    int i;
    for (i = 0; i < handler_count; i++) {
        // канал для связи источника с обработчиком
        if (pipe(pf_src_to_hdl[i]) == -1) {
            perror("Error creating pipe from src to hdl");
            return 1;
        }
        // канал для связи обработчика с приёмником
        if (pipe(pf_hdl_to_dst[i]) == -1) {
            perror("Error creating pipe from hdl to dst");
            return 1;
        }
    }

    // составляем массив аргументов для src
    char *src_argv[handler_count + 3];
    src_argv[0] = "./src";
    src_argv[1] = source_file;
    for (i = 0; i < handler_count; i++) {
        char *fd_str = (char *) malloc(32);
        sprintf(fd_str, "%d", pf_src_to_hdl[i][1]);
        src_argv[i + 2] = fd_str;
    }
    src_argv[handler_count + 2] = 0;
    // форкаем процесс для запуска src
    int src_pid;
    if ((src_pid = fork()) == -1) {
        perror("Error forking process for src");
        return 1;
    } else if (src_pid == 0) {
        execv(src_argv[0], src_argv);
        perror("Error running src");
        return 1;
    }

    // определяем размер входного файла — он нужен в dst
    FILE *in = fopen(source_file, "r");
    if (in == NULL) {
        perror("Error opening input file");
        return 1;
    }
    fseek(in, 0, SEEK_END);
    int filesize = ftell(in);
    fclose(in);
    char *filesize_str = (char *) malloc(32);
    sprintf(filesize_str, "%d", filesize);

    // составляем массив аргументов для dst
    char *dst_argv[handler_count + 4];
    dst_argv[0] = "./dst";
    dst_argv[1] = dest_file;
    dst_argv[2] = filesize_str;
    for (i = 0; i < handler_count; i++) {
        char *fd_str = (char *) malloc(32);
        sprintf(fd_str, "%d", pf_hdl_to_dst[i][0]);
        dst_argv[i + 3] = fd_str;
    }
    dst_argv[handler_count + 3] = 0;
    // форкаем процесс для запуска dst
    int dst_pid;
    if ((dst_pid = fork()) == -1) {
        perror("Error forking process for dst");
        return 1;
    } else if (dst_pid == 0) {
        execv(dst_argv[0], dst_argv);
        perror("Error running dst");
        return 1;
    }

    int hdl_pids[handler_count];

    // в цикле запускаем обработчики hdl
    for (i = 0; i < handler_count; i++) {
        // составляем массив аргументов для hdl
        char *hdl_argv[4];
        hdl_argv[0] = "./hdl";
        // канал от источника
        char *fd_src_str = (char *) malloc(32);
        sprintf(fd_src_str, "%d", pf_src_to_hdl[i][0]);
        hdl_argv[1] = fd_src_str;
        // канал в приёмник
        char *fd_dst_str = (char *) malloc(32);
        sprintf(fd_dst_str, "%d", pf_hdl_to_dst[i][1]);
        hdl_argv[2] = fd_dst_str;
        hdl_argv[3] = 0;
        // форкаем процесс для запуска hdl
        if ((hdl_pids[i] = fork()) == -1) {
            perror("Error forking process for hdl");
            return 1;
        } else if (hdl_pids[i] == 0) {
            execv(hdl_argv[0], hdl_argv);
            perror("Error running hdl");
            return 1;
        }
    }

    // ждём завершения src и закрываем все его каналы для записи
    waitpid(src_pid, NULL, 0);
    for (i = 0; i < handler_count; i++) {
        close(pf_src_to_hdl[i][0]);
    }

    // ждём завершения hdl и закрываем их каналы
    for (i = 0; i < handler_count; i++) {
        waitpid(hdl_pids[i], NULL, 0);
        close(pf_src_to_hdl[i][1]);
        close(pf_hdl_to_dst[i][0]);
    }

    // ждём завершения dst и закрываем его каналы для чтения
    waitpid(dst_pid, NULL, 0);
    for (i = 0; i < handler_count; i++) {
        close(pf_hdl_to_dst[i][1]);
    }

}