#! /bin/bash
gcc -std=c99 -D_POSIX_C_SOURCE=199309L lifesrv.c -lm -lrt -o lifesrv
gcc -std=c99 lifecln.c -o lifecln
