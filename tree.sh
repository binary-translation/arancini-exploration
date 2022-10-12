#!/bin/sh

export LD_LIBRARY_PATH=out/

<<<<<<< HEAD
make && out/txlat $1 | dot -Tx11
=======
if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

make && objdump -d ${INPUT} && out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - | dot -Tx11
>>>>>>> 78c977116688c49c95ed03741181eec0844df65b
