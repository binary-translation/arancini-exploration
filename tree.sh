#!/bin/sh

export LD_LIBRARY_PATH=out/

if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

#<<<<<<< HEAD
#make && objdump -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - #| dot -Tx11
##make && objdump -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --debug #| dot -Tx11
#=======
make && objdump -M intel -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - > ${INPUT}.dot
scripts/split_dot.py ${INPUT}.dot
echo "Generated " $(ls ${INPUT}.dot.*)
# cat ${INPUT}.dot | dot -Tx11
#>>>>>>> aa1bd47f3bb8affdbe499e37571effea44a86eae
