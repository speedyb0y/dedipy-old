#!/bin/bash

set -e

rm -f -- malloc-buffer master slave malloc.o libmalloc.so

gcc -Wall -Werror -march=native -O0 -g -c -fpic malloc.c
gcc -Wall -Werror -march=native -O0 -g -shared -o libmalloc.so malloc.o

gcc -Wall -Werror -march=native -O0 -g -fwhole-program -DLIB_MALLOC_PATH=\"$(pwd)/libmalloc.so\" master.c -o master

gcc -Wall -Werror -march=native -O0 -g -fwhole-program slave.c -o slave

# sudo  mount -t hugetlbfs nodev /opt/

fallocate -l $[2*1024*1024*1024] malloc-buffer

sudo ./master

echo $?
