#!/bin/sh

export LD_LIBRARY_PATH=out/

make && out/txlat $1 | dot -Tx11
