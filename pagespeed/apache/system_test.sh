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
# Author: abliss@google.com (Adam Bliss)
#
# Runs all Apache-specific and general system tests.
#
# See automatic/system_test_helpers.sh for usage.
#
# Expects APACHE_DEBUG_PAGESPEED_CONF to point to our config file, and
# APACHE_LOG to the log file.
#
# CACHE_FLUSH_TEST=on can be passed to test our cache.flush behavior

if [ -z $APACHE_DEBUG_PAGESPEED_CONF ]; then
  APACHE_DEBUG_PAGESPEED_CONF=/usr/local/apache2/conf/pagespeed.conf
fi

if [ -z $APACHE_LOG ]; then
  APACHE_LOG=/usr/local/apache2/logs/error_log
fi

if [ -z $APACHE_DOC_ROOT ]; then
  APACHE_DOC_ROOT=/usr/local/apache2/htdocs/
fi

PSA_JS_LIBRARY_URL_PREFIX="mod_pagespeed_static"
BEACON_HANDLER="mod_pagespeed_beacon"
STATISTICS_HANDLER="mod_pagespeed_statistics"
GLOBAL_STATISTICS_HANDLER="mod_pagespeed_global_statistics"
MESSAGES_HANDLER="mod_pagespeed_message"
HEADERS_FINALIZED=false

CACHE_FLUSH_TEST=${CACHE_FLUSH_TEST:-off}
SKIP_EXTERNAL_RESOURCE_TESTS=${SKIP_EXTERNAL_RESOURCE_TESTS:-false}
SUDO=${SUDO:-}
# TODO(jkarlin): Should we just use a vhost instead?  If so, remember to update
# all scripts that use TEST_PROXY_ORIGIN.
PAGESPEED_TEST_HOST=${PAGESPEED_TEST_HOST:-modpagespeed.com}

SERVER_NAME=apache

# Extract secondary hostname when set. Currently it's only set when doing the
# cache flush test, but it can be used in other tests we run in that run.
# Note that we use $1 not $HOSTNAME as that is only set up later by _helpers.sh.
if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  # Replace any trailing :<port> with :<secondary-port>.
  SECONDARY_HOSTNAME=${1/%:*/:$APACHE_SECONDARY_PORT}
  if [ "$SECONDARY_HOSTNAME" = "$1" ]; then
    SECONDARY_HOSTNAME=${1}:$APACHE_SECONDARY_PORT
  fi
else
  # Force the variable to be set albeit blank so tests don't fail.
  SECONDARY_HOSTNAME=${SECONDARY_HOSTNAME:-}
fi

# Inform system/system_tests.sh and the rest of this script whether statistics
# are enabled by grepping the conf file.
statistics_enabled="0"
statistics_logging_enabled="0"
if egrep -q "^    # ModPagespeedStatistics off$" \
    $APACHE_DEBUG_PAGESPEED_CONF; then
  statistics_enabled="1"
  echo STATS is ON
  if egrep -q "^ ModPagespeedStatisticsLogging on$" \
     $APACHE_DEBUG_PAGESPEED_CONF; then
    statistics_logging_enabled="1"
  fi
fi

# Tell system/system_tests.sh what our log file is.
ERROR_LOG="$APACHE_LOG"

# The 'PURGE' method is implemented, but not yet working in ngx_pagespeed, so
# have to indicate here that we want to test both PURGE and GET.  In
# nginx_system_test.sh we currently specify only GET.
#
# TODO(jmarantz) Once that's implemented in nginx, we can eliminate this
# setting.
CACHE_PURGE_METHODS="PURGE GET"

# Run General system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../system/system_test.sh" || exit 1

# TODO(jefftk): most of these tests aren't Apache-specific and should be
# slightly generalized and moved to system/ where other implementations (like
# ngx_pagespeed) can use them.

# Grab a timestamp now so that we can check that logging works.
# Also determine where the log file is.
if [ $statistics_logging_enabled = "1" ]; then
  MOD_PAGESPEED_LOG_DIR="$(
    sed -n 's/^ ModPagespeedLogDir //p' $APACHE_DEBUG_PAGESPEED_CONF |
    sed -n 's/\"//gp')"
  # Wipe the logs so we get a clean start.
  rm $MOD_PAGESPEED_LOG_DIR/*
  # The specific log file that the console will use.
  # If per-vhost stats is enabled, this is the main vhost suffix ":0".
  # If per-vhost stats is not enabled, this is the global suffix "global".
  MOD_PAGESPEED_STATS_LOG="${MOD_PAGESPEED_LOG_DIR}/stats_log_:0"
  START_TIME=$(date +%s)000 # We need this in milliseconds.
  sleep 2; # Make sure we're around long enough to log stats.
fi

SYSTEM_TEST_DIR="$(dirname "${BASH_SOURCE[0]}")/system_tests/"
run_test statistics
run_test handler_quoting
run_test loopback
run_test mod_pagespeed_message
run_test index_html_handling
run_test response_headers
run_test x_forwarded_proto
run_test beacons_load
run_test mod_rewrite

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  run_test unplugged
  run_test map_proxy_domain_for_cdn
fi

# TODO(sligocki): add test for MaxSegmentLength

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  run_test unload_handler
  run_test max_html_parse_bytes
  run_test cache_flushing
  run_test connection_refused
  run_test blocking_rewrite
  run_post_cache_flush
fi

run_test custom_fetch_headers

# Check that statistics logging was functional during these tests
# if it was enabled.
if [ $statistics_logging_enabled = "1" ]; then
  run_test statistics_logging
fi

run_test if_parsing
run_test forbid_all_disabled

# Now check stuff on secondary host.  We run this only for some tests, since we
# don't always have the secondary port number available here.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  SECONDARY_STATS_URL=http://$SECONDARY_HOSTNAME/mod_pagespeed_statistics
  SECONDARY_CONFIG_URL=$SECONDARY_STATS_URL?config

  run_test vhost_inheritance
  if [ -n "$APACHE_LOG" ]; then
    run_test encoded_absolute_urls
  fi

  run_test pass_through_headers
  run_test purging_disabled
  run_test inline_google_font_css
  run_test content_encoding_leak
fi

run_test proxying
run_test compressed_cache
run_test pagespeed_admin
run_test fetch_gzipped
run_test htaccess_override

# Cleanup
rm -rf $OUTDIR

check_failures_and_exit
