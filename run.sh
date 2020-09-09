#!/bin/bash

set -e -x


if ! [[ -n ${CC} ]] ; then
    CC="gcc"
fi

if ! [[ -n ${CC_ARGS} ]] ; then
    CC_ARGS=""
fi

if ! [[ -n ${DEDIPY_BUFFER_PATH} ]] ; then
    DEDIPY_BUFFER_PATH="/dev/hugepages/buffer"
fi

if ! [[ -n ${DEDIPY_BUFFER_SIZE} ]] ; then
    DEDIPY_BUFFER_SIZE=$[27917287424]
fi

if ! [[ -n ${DEDIPY_PROGNAME} ]] ; then
    DEDIPY_PROGNAME="dedipy"
fi

if ! [[ -n ${DEDIPY_PYTHON_PATH} ]] ; then
    DEDIPY_PYTHON_PATH="/usr/bin/python"
fi

if ! [[ -n ${DEDIPY_TEST} ]] ; then
    DEDIPY_TEST=0
fi

if ! [[ -n ${DEDIPY_DEBUG} ]] ; then
    DEDIPY_DEBUG=0
fi

if ! [[ -n ${DEDIPY_ASSERT} ]] ; then
    DEDIPY_ASSERT=0
fi

if true ; then

    CC_WARNINGS="
    -Waddress
    -Waggregate-return
    -Waggressive-loop-optimizations
    -Wconversion
    -Wdisabled-optimization
    -Wenum-compare
    -Wenum-conversion
    -Wfloat-conversion
    -Wlogical-not-parentheses
    -Wmissing-field-initializers
    -Woverflow
    -Wpointer-arith
    -Wpointer-sign
    -Wrestrict
    -Wsign-compare
    -Wsign-conversion
    -Wsizeof-array-argument
    -Wsizeof-pointer-div
    -Wsizeof-pointer-memaccess
    -Wstrict-aliasing
    -Wstring-compare
    -Wswitch-bool
    -Wswitch-outside-range
    -Wuninitialized
    -Wunused
    -Wunused-but-set-parameter
    -Wunused-but-set-variable
    -Wunused-const-variable
    -Wunused-function
    -Wunused-label
    -Wunused-local-typedefs
    -Wunused-parameter
    -Wunused-result
    -Wunused-value
    -Wunused-variable
    -Wvla
    -Wstrict-aliasing=1
    -Wstrict-overflow=5
    -Wimplicit-fallthrough=3
    -Wignored-qualifiers

    -Wabsolute-value
    -Warith-conversion
    -Wbad-function-cast
    -Wcast-align
    -Wcast-qual
    -Wclobbered
    -Wdangling-else
    -Wempty-body
    -Wendif-labels
    -Wexpansion-to-defined
    -Wimplicit-int
    -Winline
    -Wint-in-bool-context
    -Wint-to-pointer-cast
    -Wnested-externs
    -Wpadded
    -Wparentheses
    -Wpointer-compare
    -Wredundant-decls
    -Wtrigraphs
    -Wtype-limits
    -Wunsuffixed-float-constants
    -Wvector-operation-performance

    -Wno-cast-qual

    -Wno-error=inline
    -Wno-error=pointer-arith
    -Wno-error=cast-qual
    -Wno-error=sign-conversion
    -Wno-error=strict-overflow
    -Wno-error=strict-aliasing

    -fstrict-aliasing
    "
fi

#-Wwrite-strings

#-Wsuggest-attribute=[pure|const|noreturn|format|cold|malloc]

rm -f -- LOG LOG.* daemon dedipy python ${DEDIPY_PROGNAME} ${DEDIPY_PROGNAME}-daemon ${DEDIPY_PROGNAME}-python

cat > dedipy-config.c <<EOF
#define DEDIPY_PROGNAME "${DEDIPY_PROGNAME}"

#define DEDIPY_ASSERT ${DEDIPY_ASSERT}
#define DEDIPY_DEBUG ${DEDIPY_DEBUG}

#define DEDIPY_BUFFER_SIZE ${DEDIPY_BUFFER_SIZE}
#define DEDIPY_BUFFER_PATH "${DEDIPY_BUFFER_PATH}"

#define DEDIPY_PYTHON_PATH "${DEDIPY_PYTHON_PATH}"

#define DEDIPY_TEST ${DEDIPY_TEST}
#define DEDIPY_TEST_1_COUNT 500
#define DEDIPY_TEST_2_COUNT 150

#define DAEMON_CPU 1
#define DEBUG 1
EOF

# -Wextra
${CC} -std=gnu17 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Werror -Wfatal-errors -Wall -Wextra ${CC_WARNINGS} -O2 ${CC_ARGS} -fwhole-program -o ${DEDIPY_PROGNAME}-daemon dedipy-daemon.c
${CC} -std=gnu17 -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Werror -Wfatal-errors -Wall -Wextra ${CC_WARNINGS} -O2 ${CC_ARGS} -fwhole-program -o ${DEDIPY_PROGNAME}-python dedipy-python.c

# sudo  mount -t hugetlbfs nodev /opt/

if [[ ${RUN} = 1 ]] ; then

    sudo rm -f -- ${DEDIPY_BUFFER_PATH}

    sudo truncate -s ${DEDIPY_BUFFER_SIZE} ${DEDIPY_BUFFER_PATH}

    if true ; then
        sudo strace --follow-forks --output-separately -o LOG ./${DEDIPY_PROGNAME}-daemon
    else
        sudo ./${DEDIPY_PROGNAME}-daemon
    fi

    echo $?
fi
