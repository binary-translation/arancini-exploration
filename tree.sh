#! /bin/bash

# Fail on error
set -e

# Build project
cmake -B build -S .
cmake --build build -j$(nproc)

# Determine input program
if [ -z "$1" ] ; then
    INPUT="test/hello"
else
    INPUT=$1
fi

# Dump information about the input and run the translation
objdump -M intel -d ${INPUT} && build/out/txlat -I ${INPUT} -O ${INPUT}-txl --graph - > ${INPUT}.dot

# Split the DOT file
scripts/split_dot.py ${INPUT}.dot

echo "Generated " $(ls ${INPUT}.dot.*)
# cat ${INPUT}.dot | dot -Tx11
