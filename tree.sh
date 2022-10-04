#!/bin/sh

export LD_LIBRARY_PATH=out/

if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

make && out/txlat ${INPUT} | dot -Tx11
