#!/bin/bash

if [ $# -lt 2 ]; then
  echo Usage: $0 error_log_filename stop_filename
  exit 1
fi

error_log="$1"
stop_file="$2"

(tail -f $error_log | egrep "exit signal|CRASH") & background_pid=$!
while [ ! -e "$stop_file" ]; do sleep 10; done
kill $background_pid

rm -f "$stop_file"
