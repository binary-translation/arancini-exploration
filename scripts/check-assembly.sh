#! /bin/bash

if [ $# -eq 0 ]; then
    printf "Incorrect usage\n"
    printf "Usage: check-assembly.sh executable\n"
    exit 2
fi

$1 |& awk '/Current instruction/{flag=1; next} /Terminating/{flag=0} flag' - |& clang -c -x assembler -target aarch64 -o /dev/null -

