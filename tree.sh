#!/bin/sh

export LD_LIBRARY_PATH=out/

if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

make && objdump -M intel -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - > ${INPUT}.dot
scripts/split_dot.py ${INPUT}.dot
echo "Generated " $(ls ${INPUT}.dot.*)
# cat ${INPUT}.dot | dot -Tx11
