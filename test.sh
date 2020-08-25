#!/bin/bash
# EXECUTAR COMO ROOT

set -e

gcc -Wall -Werror -march=native -O0 -g -c -fpic malloc-hack.c
gcc -Wall -Werror -march=native -O0 -g -shared -o libmalloc-hack.so malloc-hack.o

LD_PRELOAD=/home/speedyb0y/libmalloc-hack.so bash


