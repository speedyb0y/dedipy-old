#!/bin/bash

set -e -x

if ! [[ -n ${CC} ]] ; then
    CC="gcc"
fi

if ! [[ -n ${CC_ARGS} ]] ; then
    CC_ARGS=""
fi

if ! [[ -n ${BUFF_PATH} ]] ; then
    BUFF_PATH="/dev/hugepages/buffer"
fi

if ! [[ -n ${BUFF_SIZE} ]] ; then
    BUFF_SIZE=$[24*1024*1024*1024+16*1024*1024]
fi

if ! [[ -n ${PROGNAME} ]] ; then
    PROGNAME="dedipy"
fi

if ! [[ -n ${DEDIPY_PYTHON_PATH} ]] ; then
    DEDIPY_PYTHON_PATH="/usr/bin/python"
fi

rm -f -- dedipy python LOG LOG.*

# -Wextra
${CC} -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DPROGNAME=\"${PROGNAME}\" -DDEBUG=1 -DDEDIPY_DEBUG=1 -DDEDIPY_VERIFY=1 -DDEDIPY_TEST=1 -DDEDIPY_PYTHON_PATH=\"${DEDIPY_PYTHON_PATH}\" -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -O2 ${CC_ARGS} -fwhole-program -o ${PROGNAME} dedipy.c

${CC} -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DPROGNAME=\"${PROGNAME}\" -DDEBUG=1 -DDEDIPY_DEBUG=1 -DDEDIPY_VERIFY=1 -DDEDIPY_TEST=1 -DDEDIPY_PYTHON_PATH=\"${DEDIPY_PYTHON_PATH}\" -DBUFF_PATH=\"${BUFF_PATH}\" -DBUFF_SIZE=${BUFF_SIZE} -Wfatal-errors -Wall -Werror -O2 ${CC_ARGS} -fwhole-program -o python python.c

# sudo  mount -t hugetlbfs nodev /opt/

if [[ ${RUN} = 1 ]] ; then

    sudo rm -f -- ${BUFF_PATH}

    if false ; then
        sudo strace --follow-forks --output-separately -o LOG ./${PROGNAME}
    else
        sudo ./${PROGNAME}
    fi

    echo $?
fi
