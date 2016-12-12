#!/bin/bash
#
# Copyright 2012 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# This scripts reads a list of URLs from the provided file, and
# fetches them in parallel from a local slurping proxy in a randomized
# order. Loading times and statuses for them are then output to
# /tmp/latency-(encoding of settings).txt

# number of fetches to do in parallel
if [ -z $PAR ]; then
  PAR=10
fi

# number of times to run
if [ -z $RUNS ]; then
  RUNS=3
fi

# How many times to repeat each trace without restarting the workers
if [ -z $EXP ]; then
  EXP=3
fi

# Proxy machine. If you specify this, make sure to give an IP address,
# as doing DNS lookups for it can slow things down a lot
if [ -z $PROXY_HOST ]; then
  PROXY_HOST=127.0.0.1
fi

# .. and port
if [ -z $PROXY_PORT ]; then
  PROXY_PORT=8080
fi

# Extra flags to pass to fetch_all.py
FLAGS=${FLAGS:-}

USER_AGENT_FLAG=${USER_AGENT:+--user_agent}

if [ $# -lt 1 ]; then
  echo "Usage: devel/trace_stress_test.sh urls_file ..."
  echo "Shuffles each urls_file in turn, runs through shuffled file using"
  echo "$PAR parallel wget jobs.  Repeats this process $RUN times."
  exit 2
fi

OUR_PATH=`dirname $0`
STAMP=`date +%Y%m%d-%H%M`
LATENCY_REPORT=/tmp/latency-$PROXY_HOST-R$RUNS-P$PAR-E$EXP-$STAMP.txt
TAIL_HEAD_TEMP=/tmp/tail_head.$$

echo "time status url" > $LATENCY_REPORT

# Examines file in $1, starting at line $2, and the next $3 lines into file $4.
function tail_head {
  input_file=$1
  start_pos=$2
  num_lines=$3
  outfile=$4

  # We make a temp file because otherwise we (at least Josh) get a lot of
  # "tail: write error" printed out.
  tail $input_file -n +$start_pos < $input_file > $TAIL_HEAD_TEMP
  head $TAIL_HEAD_TEMP -n $num_lines >$outfile
}

function single_run {
  FILE=$1
  # Shuffle the log and split it into pieces
  SHUF_FILE=`mktemp`
  for I in `seq 1 $EXP`; do
    shuf $FILE >> $SHUF_FILE
  done
  LINES=`wc -l $SHUF_FILE | sed s#$SHUF_FILE##`
  # Setting chunk size slightly too large balances load a little better, most
  # obvious when $LINES < $PAR.
  CHUNK=`expr 1 + $LINES / $PAR`

  # feed each chunk to a separate wget
  PIECES=
  LOGS=
  POS=0
  for I in `seq 1 $PAR`; do
    CUR_CHUNK=$CHUNK
    if [ $I -eq $PAR ]; then
      # make sure we also include the remainder
      EXTRA=`expr $LINES - $PAR \* $CHUNK`
      CUR_CHUNK=`expr $CUR_CHUNK + $EXTRA`
    fi
    PIECE=`mktemp`
    LOG=`mktemp`
    PIECES="$PIECES $PIECE"
    LOGS="$LOGS $LOG"
    tail_head $SHUF_FILE $POS $CUR_CHUNK $PIECE
    $OUR_PATH/fetch_all.py $FLAGS $USER_AGENT_FLAG $USER_AGENT \
        --proxy_host $PROXY_HOST --proxy_port $PROXY_PORT \
        --urls_file $PIECE &> $LOG &
    POS=`expr $POS + $CHUNK`
  done

  # Wait for all to finish
  wait

  # Print out the summary messages
  cat $LOGS >> $LATENCY_REPORT

  # clean up
  rm $PIECES
  rm $LOGS
  rm $SHUF_FILE
}

START=$SECONDS

for RUN in `seq 1 $RUNS`; do
  echo "Run $RUN"
  for FILE in "$@"; do
    echo "File $FILE"
    single_run "$FILE"
  done
  echo "----------------------------------------------------------------------"
done

STOP=$SECONDS
LINES=`tail -n +2 $LATENCY_REPORT|wc -l`
ELAPSED=`expr $STOP - $START`
QPS=`expr $LINES / $ELAPSED`
echo "QPS estimate (inaccurate for short runs):" $QPS "requests/sec"
echo
$OUR_PATH/trace_stress_test_percentiles.sh $LATENCY_REPORT | cut -c 1-80
echo
echo "10 worst latencies:"
head -n 10 ${LATENCY_REPORT%%.txt}-sorted.txt
echo
echo "Status statistics:"
tail -n +2 $LATENCY_REPORT | cut -d ' ' -f 2 | sort | uniq -c
echo "Full latency report in:" $LATENCY_REPORT

rm -f $TAIL_HEAD_TEMP

