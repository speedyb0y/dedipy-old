#!/bin/bash

set -e

rm -f -- malloc-buffer master slave malloc.o libmalloc.so LOG.*

gcc -Wfatal-errors -Wall -Werror -march=native -O0 -g -c -fpic malloc.c
gcc -Wfatal-errors -Wall -Werror -march=native -O0 -g -shared -o libmalloc.so malloc.o

gcc -Wfatal-errors -Wall -Werror -march=native -O0 -g -fwhole-program -DLIB_MALLOC_PATH=\"$(pwd)/libmalloc.so\" master.c -o master

gcc -Wfatal-errors -Wall -Werror -march=native -O0 -g -fwhole-program slave.c -o slave

# sudo  mount -t hugetlbfs nodev /opt/

fallocate -l $[128*1024*1024] malloc-buffer

sudo strace --follow-forks --output-separately -o LOG ./master

echo $?
