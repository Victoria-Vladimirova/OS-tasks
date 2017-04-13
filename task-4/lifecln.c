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
#include <stdio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char ** argv) {

	// должно быть 2 аргумента
	if (argc != 3) {
		printf("Usage: ./lifecln <host> <port>\n");
		return 1;
	}

	// имя хоста
	char * hostname = argv[1];
	// номер порта
	int port = atoi(argv[2]);

	// получаем информацию о хосте по его имени
	struct hostent * host = gethostbyname(argv[1]);

	// создаём клиентский сокет
	int sock_client;
	if ((sock_client = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		printf("Error creating socket\n");
		return 1;
	}

	// заполняем структуру адреса для подключения
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr = * (struct in_addr*) host->h_addr_list[0];
	sin.sin_port = htons(port);

	// подключаемся к удалённому сокету
	if (connect(sock_client, (struct sockaddr *) &sin, sizeof(sin)) == -1) {
		printf("Cannot connect to %s:%d\n", hostname, port);
		return 1;
	}

	// выводим полученные данные
	char ch;
	while(read(sock_client, &ch, 1) > 0) {
		printf("%c", ch);
	}

	// закрываем клиентский сокет и выходим
	close(sock_client);
	return 0;

}