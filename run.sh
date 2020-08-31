#!/bin/bash

set -e

BUFF_PATH="/tmp/malloc-buffer"
BUFF_SIZE=$[4*1024*1024*1024+16*1024*1024]

rm -f -- ${BUFF_PATH}
rm -f -- master slave malloc.o libmalloc.so LOG.*

gcc -DDEBUG=1 -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -march=native -O0 -g -c -fpic malloc.c
gcc -DDEBUG=1 -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -march=native -O0 -g -shared -Wl,-init,ms_init -o libmalloc.so malloc.o

gcc -DDEBUG=1 -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -march=native -O0 -g -fwhole-program -DLIB_MALLOC_PATH=\"$(pwd)/libmalloc.so\" master.c -o master

gcc -DDEBUG=1 -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -march=native -O0 -g -fwhole-program slave.c -o slave

# sudo  mount -t hugetlbfs nodev /opt/

fallocate -l ${BUFF_SIZE} ${BUFF_PATH}

sudo strace --follow-forks --output-separately -o LOG ./master

echo $?
