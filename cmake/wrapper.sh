#!/usr/bin/env bash
declare path=$1
declare exec=$2
shift 2
if [ ! -e "$path" ]; then
  echo "$path not found"
  $exec "$@"
fi