### sparse-files

Дополнение к gzip, которому на вход подается распакованный на стандартный вывод gzip’ом разрежённый файл.

```
gcc -o sparser task_2.c
Запуск: gzip -cd sparse-file.gz | ./sparser <filename>
```

CentOS 7, GNU bash version 4.2.46