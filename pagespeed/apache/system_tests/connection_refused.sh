#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# connection_refused.html references modpagespeed.com:1023/someimage.png.
# mod_pagespeed will attempt to connect to that host and port to fetch the
# input resource using serf.  We expect the connection to be refused.  Relies
# on "ModPagespeedDomain modpagespeed.com:1023" in debug.conf.template.  Also
# relies on running after a cache-flush to avoid bypassing the serf fetch,
# since mod_pagespeed remembers fetch-failures in its cache for 5 minutes.

if ! "$SKIP_EXTERNAL_RESOURCE_TESTS" && \
   [ "${FIRST_RUN:-}" = "true" ] && [ "${VIRTUALBOX_TEST:-}" = "" ]; then
  start_test Connection refused handling

  # Monitor the Apache log starting now.  tail -F will catch log rotations.
  SERF_REFUSED_PATH=$TESTTMP/instaweb_apache_serf_refused
  rm -f $SERF_REFUSED_PATH
  echo APACHE_LOG = $APACHE_LOG
  tail --sleep-interval=0.1 -F $APACHE_LOG > $SERF_REFUSED_PATH &
  TAIL_PID=$!

  # Wait for tail to start.
  echo -n "Waiting for tail to start..."
  while [ ! -s $SERF_REFUSED_PATH ]; do
    sleep 0.1
    echo -n "."
  done
  echo "done!"

  # Actually kick off the request.
  echo $WGET_DUMP $TEST_ROOT/connection_refused.html
  echo checking...
  check $WGET_DUMP $TEST_ROOT/connection_refused.html > /dev/null
  echo check done
  # If we are spewing errors, this gives time to spew lots of them.
  sleep 1
  # Wait up to 10 seconds for the background fetch of someimage.png to fail.
  for i in {1..100}; do
    ERRS=$(grep -c "Serf status 111" $SERF_REFUSED_PATH || true)
    if [ $ERRS -ge 1 ]; then
      break;
    fi;
    echo -n "."
    sleep 0.1
  done;
  echo "."
  # Kill the log monitor silently.
  kill $TAIL_PID
  wait $TAIL_PID 2> /dev/null || true
  check [ $ERRS -ge 1 ]
  # Make sure we have the URL detail we expect because
  # ModPagespeedListOutstandingUrlsOnError is on in debug.conf.template.
  echo Check that ModPagespeedSerfListOutstandingUrlsOnError works
  check grep "URL http://modpagespeed.com:1023/someimage.png active for " \
      $SERF_REFUSED_PATH
fi
