#!/bin/sh

export LD_LIBRARY_PATH=out/

make && out/txlat test/hello | dot -Tx11
