#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Usage: ./expect.sh URL COMMAND RESULT
# Continously fetches URL and pipes the output to COMMAND.  Loops until
# COMMAND outputs RESULT, in which case we return 0, or until 10 seconds have
# passed, in which case we return 1.

URL=$1
COMMAND=$2
RESULT=$3

TIMEOUT=10
START=`date +%s`
STOP=$((START+$TIMEOUT))

echo Fetching $URL until '`'$COMMAND'`' = $RESULT
while test -t; do
  if [ `wget -q -O - $URL 2>&1 | $COMMAND` = $RESULT ]; then
    /bin/echo "Success."
    exit 0;
  fi;
  if [ `date +%s` -gt $STOP ]; then
    /bin/echo "Timeout."
    exit 1;
  fi;
  /bin/echo -n "."
  sleep 0.1
done;

