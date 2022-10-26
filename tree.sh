#!/bin/sh

export LD_LIBRARY_PATH=out/

if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

make && objdump -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - #| dot -Tx11
#make && objdump -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --debug #| dot -Tx11
