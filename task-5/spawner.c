#include <stdio.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>

// максимальное количество команд в конфигурационном файле
#define MAX_COMMANDS 10
// максимальное количество неуспешных попыток запуска
#define MAX_FAILED_RUNS 50
// срок блокировки некорректно запущенной программы, в секундах
#define FAILED_BLOCK_TIME 5
// путь к конфигурационному файлу
#define SPAWNER_CFG_FILE "/etc/spawner.cfg"
// шаблон пути к файлам pid
#define SPAWNER_PID_FILE "/tmp/spawner-%d.pid"

// режимы запуска команд: ожидание завершения, перезапуск
enum wait_mode {
    WAIT, RESPAWN
};

char *STR_WAIT = "wait";
char *STR_RESPAWN = "respawn";

// структура, описывающая команду
struct command {
    // команда и её аргументы
    char *args[10];
    // режим завершения команды
    enum wait_mode mode;
    // идентификатор процесса
    pid_t pid;
    // количество неудачных запусков подряд
    int failed_runs;
    // время, когда программа была заблокирована
    time_t block_time;
};

// массив команд, прочитанный из файла
struct command *commands[MAX_COMMANDS];
int command_count = 0;
int run_count = 0;
int need_reload = 1;

// демонизация процесса
void daemonize() {

    int fd;
    struct rlimit flim;

    if (getppid() != 1) {
        // если родительский процесс - не init, то нужно заигнорить терминальные сигналы
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);

        // порождаем дочерний процесс, чтобы сделать его лидером, а родительский завершить
        if (fork() != 0) {
            exit(0);
        }
        // делаем дочерний процесс лидером
        setsid();
    }

    // закрываем все файловые дескрипторы
    getrlimit(RLIMIT_NOFILE, &flim);
    for (fd = 0; fd < flim.rlim_max; fd++) {
        close(fd);
    }

    // меняем текущий каталог на корневой, т.к. он всегда принадлежит текущей файловой системе
    chdir("/");

    // пишем сообщение о запуске в лог
    syslog(LOG_INFO, "Spawner Daemon Launched");

}

// чтение конфигурационного файла
void read_config() {
    syslog(LOG_INFO, "Reading configuration file");

    FILE *conf = fopen(SPAWNER_CFG_FILE, "r");

    if (conf == NULL) {
        syslog(LOG_ERR, "Cannot read file %s", SPAWNER_CFG_FILE);
        return;
    }

    char *line = NULL;
    size_t linecap = 0;
    ssize_t linelen;
    command_count = 0;

    while ((linelen = getline(&line, &linecap, conf)) > 0) {

        if (command_count == MAX_COMMANDS) {
            syslog(LOG_WARNING, "Too many commands. Max allowed = %d", MAX_COMMANDS);
            free(line);
            line = NULL;
            break;
        }

        // разбираем строку по пробелам
        char *initial_line = line;

        struct command *acommand = malloc(sizeof(struct command));
        memset(acommand, 0, sizeof(struct command));

        char **ap;
        char **argv = acommand->args;
        for (ap = argv; (*ap = strsep(&line, " \n")) != NULL;) {
            if (**ap != '\0') {
                if (++ap >= &argv[10]) {
                    break;
                }
            }
        }

        // после выхода из цикла ap ссылается на следующий после последнего элемент массива argv
        // находим количество элементов — должно быть как минимум 2 (название команды, режим)
        int argc = ap - argv;

        if (argc < 2) {
            syslog(LOG_WARNING, "Cannot parse command string: %s", initial_line);
            free(initial_line);
            line = NULL;
            continue;
        }

        // разбираем режим - последнюю строку из массива argv
        enum wait_mode mode;
        char *mode_str = argv[argc - 1];
        if (strcmp(STR_WAIT, mode_str) == 0) {
            mode = WAIT;
        } else if (strcmp(STR_RESPAWN, mode_str) == 0) {
            mode = RESPAWN;
        } else {
            syslog(LOG_WARNING, "Unknown mode: %s", mode_str);
            free(initial_line);
            line = NULL;
            continue;
        }

        // записываем в последний элемент массива 0, чтобы не передавать режим аргументом в вызов программы
        argv[argc - 1] = 0;

        // заполняем структуру с командой
        acommand->mode = mode;
        commands[command_count++] = acommand;

        line = NULL;
    }
    fclose(conf);
}

void inc_and_check_failed_runs(struct command *acommand) {
    acommand->failed_runs++;
    // syslog(LOG_WARNING, "Command %s failed for %d time", acommand->args[0], acommand->failed_runs);
    printf("Command %s failed for %d time\n", acommand->args[0], acommand->failed_runs);
    if (acommand->failed_runs > MAX_FAILED_RUNS) {
        acommand->failed_runs = 0;
        acommand->block_time = time(NULL);
    }
}

// запуск команды из массива commands под номером num
void fork_command(int num) {
    pid_t cpid = fork();
    switch (cpid) {
        case -1:
            syslog(LOG_ERR, "Failed to fork the parent process for %s", commands[num]->args[0]);
            inc_and_check_failed_runs(commands[num]);
            break;
        case 0:;
            struct command *acommand = commands[num];
            char **args = &acommand->args[0];
            execv(args[0], args);
            // вызов execve() должен был заместить образ процесса.
            // если мы попали сюда, то это ошибка
            syslog(LOG_ERR, "Failed to start process for %s", commands[num]->args[0]);
            exit(1);
        default:
            // сохраняем pid форкнутого процесса
            syslog(LOG_INFO, "Forked process %d", cpid);
            run_count++;
            commands[num]->pid = cpid;

            // создаём pid-файл /tmp/spawner-*.pid и сохраняем в него pid
            char fname[20];
            sprintf(fname, SPAWNER_PID_FILE, num);
            FILE *fd = fopen(fname, "w");
            if (fd == NULL) {
                syslog(LOG_ERR, "Failed to create pid file %s", fname);
            } else {
                fwrite(&cpid, 4, 1, fd);
                fclose(fd);
            }
    }
}

// обработчик сигнала HUP
void hup_handler(int signum) {
    syslog(LOG_INFO, "HUP signal received");
    need_reload = 1;
}

// установка обработчика сигнала HUP
void install_hup_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &hup_handler;
    sigaction(SIGHUP, &sa, NULL);
}

void reload() {
    syslog(LOG_INFO, "Reloading the initial data");

    int i;
    for (i = 0; i < command_count; i++) {
        // если процесс запущен, прибиваем его
        if (commands[i]->pid != 0) {
            syslog(LOG_INFO, "Killing child process %d", commands[i]->pid);
            kill(commands[i]->pid, SIGKILL);
            run_count--;
        }
    }

    // перечитывание конфига
    read_config();

    // первоначальный запуск всех процессов
    for (i = 0; i < command_count; i++) {
        fork_command(i);
    }
    need_reload = 0;
}

void rerun_if_needed(int num) {
    struct command *acommand = commands[num];
    if (acommand->mode == RESPAWN && acommand->block_time == 0) {
        // продолжаем перезапускать задачи с mode = RESPAWN
        syslog(LOG_INFO, "Command %s set for restart, respawning", acommand->args[0]);
        fork_command(num);
    }
}

void wait_for_process_termination() {
    int status;
    pid_t cpid = waitpid(-1, &status, 0);
    int i;
    for (i = 0; i < command_count; i++) {
        if (commands[i]->pid == cpid) {
            if (WIFEXITED(status) == 1) {
                if (WEXITSTATUS(status) == 0) {
                    // если программа завершилась успешно, сбрасываем счётчик неудачных запусков
                    commands[i]->failed_runs = 0;
                } else {
                    // если программа завершилась неуспешно, увеличиваем счётчик неудачных запусков
                    inc_and_check_failed_runs(commands[i]);
                }
            }
            // удаляем pid-файл в /tmp
            char fname[20];
            sprintf(fname, "/tmp/spawner-%d.pid", i);
            unlink(fname);

            syslog(LOG_INFO, "Child process %d terminated", cpid);
            run_count--;
            rerun_if_needed(i);
            break;
        }
    }
}

// обработчик сигнала таймера
void timer_handler(int signum) {
    int i;
    for (i = 0; i < command_count; i++) {
        printf("num %d, block_time %d, diff %d,	 failed runs %d\n", i, commands[i]->block_time,
               time(NULL) - commands[i]->block_time > FAILED_BLOCK_TIME, commands[i]->failed_runs);
        if (commands[i]->block_time != 0 && time(NULL) - commands[i]->block_time > FAILED_BLOCK_TIME) {
            syslog(LOG_INFO, "Block expired for command %s, resetting", commands[i]->args[0]);
            commands[i]->block_time = 0;
            rerun_if_needed(i);
        }
    }
}

// установка таймера с интервалом в 1 секунду
int install_timer() {
    struct sigaction sa;
    struct itimerval timer;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    sigaction(SIGALRM, &sa, NULL);
    timer.it_value.tv_sec = 1;
    timer.it_value.tv_usec = 0;
    timer.it_interval = timer.it_value;
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        printf("Error setting timer\n");
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {

    openlog("spawner", LOG_PID | LOG_CONS, LOG_DAEMON);

    // демонизируем процесс, если есть соответствующий ключ запуска
    if (argc == 2 && !strcmp("--daemonize", argv[1])) {
        daemonize();
    }

    // устанавливаем обработчик сигнала HUP
    install_hup_handler();

    // устанавливаем таймер, чтобы проверять заблокированные задачи
    install_timer();

    // основной цикл работы приложения
    while (1) {

        // если пришёл сигнал HUP, то перезагружаем приложение
        if (need_reload) {
            reload();
        }

        // ожидаем завершения одного процесса и обрабатываем его
        wait_for_process_termination();

        // если все процессы выполнены, и не требуется перечитывание конфига, то выходим
        if (!run_count && !need_reload) {
            break;
        }
    }

    closelog();
}