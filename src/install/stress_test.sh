#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Usage: ./stress_test.sh HOSTPORT
# Stress-tests a mod_pagespeed installation.  This currently takes about 15sec.
# Exits with status 0 if all tests pass.  Exits 1 immediately if any test fails.
# You should probably wipe out your cache and restart the server before starting
# the test.

if [ $# != 1 ]; then
  echo Usage: ./stress_test.sh HOSTPORT;
  exit 2;
fi;
HOSTPORT=$1

TEST_DIR=/tmp/mod_pagespeed_stress_test.$USER;
mkdir -p $TEST_DIR;
cd $TEST_DIR;

echo "Starting 10 simultaneous recursive wgets"
X=0;
PIDS="";
while [ $X -lt 10 ]; do
  wget -q -P $X -p http://$HOSTPORT/mod_pagespeed_example/stress_test.html &
  PIDS="$PIDS $!";
  X=$((X+1));
done;

# Monitor the number of processes for 10 seconds
echo;
echo;
MAX=0;
X=22;

if [ $TERM == dumb ]; then
  OVERWRITE=""
else
  OVERWRITE="\033[2A"
fi

while [ $X -ge 0 ]; do
  NUM=` ps -efww|egrep 'bin/[h]ttpd|bin/[a]pache'|wc -l`
  if [ $NUM -gt $MAX ]; then
    MAX=$NUM;
  fi;
  /bin/echo -e "${OVERWRITE}Apache processes:  $NUM   ";
  echo "Time remaining:  $((X/2))   "
  sleep 0.5;
  X=$((X-1))
done;

echo "Test complete; killing wgets"
kill $PIDS 2>/dev/null;

if [  $MAX -gt 100 ]; then
  echo "FAIL:  $MAX processes were spawned.";
  RETURN_VAL=1;
else
  echo "PASS."
  RETURN_VAL=0;
fi;

exit $RETURN_VAL;
