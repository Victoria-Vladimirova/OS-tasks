### Компиляция
`gcc spawner.c -o spawner`
### Конфигурация
Конфигурационный файл помещается в `/etc/spawner.cfg` и имеет вид:
```
/bin/ls -l wait
/bin/sleep 3 spawn
```
Где команда и её аргументы разделены пробелами. Последний параметр имеет значения:
* `wait` — однократно запустить программу и ждать завершения
* `respawn` — после завершения программы перезапустить её

### Запуск
В режиме демона (все сообщения программы пишутся в syslog, сообщения запущенных программ игнорируются):
`./spawner --daemonize`
В режиме консольной утилиты (все сообщения программы пишутся в syslog, сообщения запущенных программ пишутся в консоль):
`./spawner`

### Параметры в коде
```
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
```