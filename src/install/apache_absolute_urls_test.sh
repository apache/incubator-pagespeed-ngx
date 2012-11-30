#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Tests requests for encoded absolute urls are not respected.
#
# See system_test_helpers.sh for usage.

this_dir=$(dirname $0)
source "$this_dir/system_test_helpers.sh" || exit 1

start_test Encoded absolute urls are not respected.

# Monitor the Apache log; tail -F will catch log rotations.
ABSOLUTE_URLS_LOG_PATH=/tmp/instaweb_apache_absolute_urls_log.$$
echo APACHE_LOG = $APACHE_LOG
tail --sleep-interval=0.1 -F $APACHE_LOG > $ABSOLUTE_URLS_LOG_PATH &
TAIL_PID=$!

# should fail; the example.com isn't us.
check_not $WGET_DUMP "$HOSTNAME/,hexample.com.pagespeed.jm.0.js"

REJECTED="Rejected absolute url reference"

# Wait up to 10 seconds for failure.
for i in {1..100}; do
  REJECTIONS=$(fgrep -c "$REJECTED" $ABSOLUTE_URLS_LOG_PATH)
  if [ $REJECTIONS -ge 1 ]; then
    break;
  fi;
  /bin/echo -n "."
  sleep 0.1
done;
/bin/echo "."

# Kill the log monitor silently.
kill $TAIL_PID
wait $TAIL_PID 2> /dev/null

check [ $REJECTIONS -eq 1 ]

system_test_trailer
