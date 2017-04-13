/**********************************************************
 * Клиентская часть приложения
 *
 * Сокеты. Программа-сервер должна строго раз в 1
 * секунду пересчитывать состояние Game of Life. Если
 * программа не успевает это делать, она должна
 * выдавать сообщение об ошибке.
 * • Сервер должен выдавать по запросу пользователя
 * (программа-клиент) последнее из вычисленных состояний
 * Game of Life.
 * • Начальный шаблон желательно считывать из файла.
 **********************************************************/

#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <math.h>

// размер очереди запросов к серверному сокету
#define QUEUE_LENGTH 10
// максимальный размер поля (100х100)
#define MAX_FIELD_SIZE 100
// интервал пересчёта поля в секундах
#define RECALCULATE_INTERVAL 1
// порт сервера
#define PORT 8080
// символ занятой ячейки
#define CELL 'X'
// символ свободной ячейки
#define ZERO '-'

// высота и ширина поля
int height, width;
// игровое поле
char ** field;
char ** tmp_field;

// вывести поле в указанный дескриптор
void print_field(FILE * fd) {
	for (int i = 0; i < height; i++) {
		fprintf(fd, "%s", field[i]);
	}
	fprintf(fd, "\n");
}

// считать файл с именем name в поле field
int read_file(char * name) {
	// открываем файл
	int file;
	if ((file = open(name, O_RDONLY)) == -1) {
		printf("Failed to open input file\n");
		return 1;
	}

	// считываем поле
	char * linebuf = (char *) malloc(MAX_FIELD_SIZE);
	field = (char**) malloc(MAX_FIELD_SIZE);
	tmp_field = (char**) malloc(MAX_FIELD_SIZE);

	char ch;
	int colidx = 0;
	while (read(file, &ch, 1) > 0) {
		if (ch == '\n') {
			// контролируем, что все строки поля имеют одинаковую ширину
			if (width == 0) {
				width = colidx;
			} else if (colidx == 0) {
				// если столкнулись с пустой строкой (например, в конце файла), игнорируем её
				continue;
			} else if (width != colidx) {
				printf("The field rows should have the same length\n");
				return -1;
			}
			linebuf[colidx] = '\n';
			linebuf[colidx + 1] = '\0';
			char * line = (char *) malloc(colidx + 1);
			strcpy(line, linebuf);
			field[height] = line;

			// создаём такую же структуру для временного поля
			char * tmp_line = (char *) malloc(colidx + 1);
			tmp_field[height] = tmp_line;

			height++;

			colidx = 0;

		} else if (ch == CELL || ch == ZERO) {
			linebuf[colidx++] = ch;
		} else {
			printf("Bad character in file: %c\n", ch);
			return -1;
		}
	} 

	// закрываем файл
	close(file);
	free(linebuf);
	return 0;
}

long time_micros() {
	struct timespec spec;
    clock_gettime(CLOCK_REALTIME, &spec);
    return round(spec.tv_nsec / 1000) + spec.tv_sec * 1000000;
}

// пересчёт игрового поля
void calculate_field() {
	static int count = 0;

    long ms_start = time_micros();

	time_t t = time(NULL);
	printf("Starting recalculation %d at %ld\n", count, ms_start);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			// считаем суммарное количество клеток в соседних ячейках
			int count =
			(i > 0 && j > 0 && field[i - 1][j - 1] == CELL)
			+ (i > 0 && field[i - 1][j] == CELL)
			+ (i > 0 && j < width - 1 && field[i - 1][j + 1] == CELL)
			+ (j < width - 1 && field[i][j + 1] == CELL)
			+ (i < height - 1 && j < width - 1 && field[i + 1][j + 1] == CELL)
			+ (i < height - 1 && field[i + 1][j] == CELL)
			+ (i < height - 1 && j > 0 && field[i + 1][j - 1] == CELL)
			+ (j > 0 && field[i][j - 1] == CELL);

			if (field[i][j] == ZERO && count == 3) {
				// если текущая ячейка не занята, а рядом находятся ровно 3 клетки - рождается новая
				tmp_field[i][j] = CELL;
			} else if (field[i][j] == CELL && (count < 2 || count > 3)) {
				// если текущая ячейка занята, а рядом менее 2 или более 3 клеток — текущая клетка погибает
				tmp_field[i][j] = ZERO;
			} else {
				// в противном случае всё остаётся как прежде
				tmp_field[i][j] = field[i][j];
			}

			// контролируем, что не прошла 1 секунда
			long ms_current = time_micros();
			if (ms_current - ms_start > 1000000) {
				printf("Failed to recalcuate on time.\n");
				return;
			}
		}
	}
	// копируем временное поле в основное
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			field[i][j] = tmp_field[i][j];
		}
	}

	long ms_end = time_micros();

	printf("Finished recalculation %d at %ld, %ld microseconds\n", count, ms_end, ms_end - ms_start);

	count++;

}

// обработчик сигнала таймера
void timer_handler (int signum) {
	calculate_field();
	print_field(stdout);
}

// установка таймера с интервалом в 1 секунуд
int install_timer() {
	struct sigaction sa;
 	struct itimerval timer;
 	memset(&sa, 0, sizeof (sa));
 	sa.sa_handler = &timer_handler;
 	sigaction(SIGALRM, &sa, NULL);
 	timer.it_value.tv_sec = RECALCULATE_INTERVAL;
 	timer.it_value.tv_usec = 0;
 	timer.it_interval = timer.it_value;
 	if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
 		printf("Error setting timer\n");
 		return -1;
 	}
 	return 0;
}

int main(int argc, char ** argv) {

	// проверяем аргументы - обязательно должно быть имя файла
	if (argc != 2) {
		printf("Usage: ./lifesrv <filename>\n");
		return 1;
	}

	if (read_file(argv[1]) == -1) {
		return 1;
	}

	// выводим первоначальное состояние поля
	print_field(stdout);

	// устанавливаем таймер на 1 секунду
	if (install_timer() == -1) {
		return 1;
	}

	// создаём серверный сокет
	int sock_server;
	if ((sock_server = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Error creating the socket\n");
		return 1;
	}

	// получаем адрес текущего хоста
	struct hostent * host = gethostbyname("0.0.0.0");

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr = * (struct in_addr*) host->h_addr_list[0];
	sin.sin_port = htons(PORT);

	if (bind(sock_server, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		printf("Error binding the socket\n");
		return 1;
	}

	// переводим сокет в состояние listen
	if (listen(sock_server, QUEUE_LENGTH) == -1) {
		printf("Error listening to the socket\n");
		return 1;
	}

	// цикл обработки запросов от клиентов
	int sock_client;
	while (1) {

		if ((sock_client = accept(sock_server, NULL, NULL)) == -1) {
			// игнорируем прерывание от таймера
			if (errno == EINTR) {
				continue;
			}
			printf("Error accepting the socket\n");
			return 1;
		}

		for (int i = 0; i < height; i++) {
			if (write(sock_client, field[i], width + 1) == -1) {
				printf("Error writing to the socket\n");
			}
		}
		close(sock_client);

	}

	// освобождаем память
	for (int i = 0; i < height; i++) {
		free(field[i]);
		free(tmp_field[i]);
	}
	free(field);
	free(tmp_field);

	// закрываем сокет
	close(sock_server);

	return 0;

}
