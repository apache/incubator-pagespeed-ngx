#!/bin/bash
#
# Copyright 2010 Google Inc.
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
#
# Author: morlovich@google.com (Maks Orlovich)
#
# Runs the measurement proxy tests.  This is isolated in a separate file
# because this is also testing a deprecated feature of enabling gzip
# in a vhost without the inherit flags.  This produces ugly warnings on
# apache restart that I want to move out of the default path.

# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test_helpers.sh" || exit 1

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test Measurement proxy mode
  # Wrong password --- 403.
  OUT=$($CURL --silent --include --proxy $SECONDARY_HOSTNAME\
      http://mpr.example.com/h/_/sicrit/www.modpagespeed.com/)
  check_from "$OUT" fgrep -q "403 "

  # Right one...
  OUT=$($CURL --silent --include --proxy $SECONDARY_HOSTNAME\
      http://mpr.example.com/h/b/secret/www.gstatic.com/psa/static/57ea3579bb97c7b4ca6658061d5c765b-mobilize.js)
  # Note that this also checks that it got minified --- the original has
  # spaces around the equal sign.
  check_from "$OUT" fgrep -q "goog.constructNamespace_="

  # Combined + minified CSS.
  OUT=$($CURL --silent --include --proxy $SECONDARY_HOSTNAME\
      http://mpr.example.com/h/b/secret/www.gstatic.com/psa/static/A.0e5d6484d7bf84edf94c17a8d6a6c6de-mobilize.css+0f58e3ef023072001e64bba88abaeeeb-mobilize.css,Mcc.JzQiGpc0_X.css.pagespeed.cf.0slDU6deBr.css)
  check_from "$OUT" fgrep -q "psmob-map-button"

  start_test Measurement proxy mode passthrough gzip
  URL="http://mprpass.example.com/h/b/secret/www.gstatic.com/psa/static/57ea3579bb97c7b4ca6658061d5c765b-mobilize.js"
  http_proxy=${SECONDARY_HOSTNAME} fetch_until -gzip $URL "wc -c" 69551 "" -le
  start_test Measurement proxy mode rewrite gzip
  URL="http://mpr.example.com/h/b/secret/www.gstatic.com/psa/static/57ea3579bb97c7b4ca6658061d5c765b-mobilize.js"
  http_proxy=${SECONDARY_HOSTNAME} fetch_until -gzip $URL "wc -c" 65151 "" -le
  start_test Measurement proxy mode passthrough nogzip
  URL="http://mprpass.example.com/h/b/secret/www.gstatic.com/psa/static/57ea3579bb97c7b4ca6658061d5c765b-mobilize.js"
  http_proxy=${SECONDARY_HOSTNAME} fetch_until $URL "wc -c" 288467 "" -ge
  start_test Measurement proxy mode rewrite nogzip
  URL="http://mpr.example.com/h/b/secret/www.gstatic.com/psa/static/57ea3579bb97c7b4ca6658061d5c765b-mobilize.js"
  http_proxy=${SECONDARY_HOSTNAME} fetch_until $URL "wc -c" 241055 "" -ge
fi

start_test Efficacy of ModPagespeedFetchWithGzip

# TODO(sligocki): The serf_fetch_bytes_count should be available on
# this vhost's pagespeed_admin/statistics page. Why isn't it?
STATS=$OUTDIR/gzip_efficacy_stats
$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.1

# Note: The client request will not served with gzip because we do not
# have an Accept-Encoding header, we are testing that the backend fetch
# uses gzip.
EXAMPLE_BIG_CSS="$EXAMPLE_ROOT/styles/big.css.pagespeed.ce.01O-NppLwe.css"
echo $WGET -O /dev/null --save-headers "$EXAMPLE_BIG_CSS"
$WGET -O /dev/null --save-headers "$EXAMPLE_BIG_CSS" 2>&1 \
  | head | grep "HTTP request sent, awaiting response... 200 OK"
$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.2
check_stat_op $STATS.1 $STATS.2 serf_fetch_bytes_count 200 -gt
check_stat_op $STATS.1 $STATS.2 serf_fetch_bytes_count 500 -lt

check_failures_and_exit
