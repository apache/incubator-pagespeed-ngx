#!/bin/bash
#
# Copyright 2013 Google Inc.
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
# Author: anupama@google.com (Anupama Dutta)
#
# Runs all Apache-specific downstream caching tests.
#
# Environment variables that are used by this test:
# 1) VARNISH_SERVER should be set to the varnish caching layer
# host:port for testing to be complete. If VARNISH_SERVER is empty,
# minimal testing of the feature is done.
# 2) MUST_BACKUP_DEFAULT_VCL should be set to 1 or 0 depending on
# whether we want the existing default.vcl to be backed up and restored
# before and after the test or it can be overwritten by the updated
# debug_conf.v3.vcl. By default, it is assumed to have the value 1.

this_dir="$( dirname "${BASH_SOURCE[0]}" )"
PAGESPEED_CODE_DIR="$this_dir/../../../third_party/pagespeed"
if [ ! -e "$PAGESPEED_CODE_DIR" ] ; then
  PAGESPEED_CODE_DIR="$this_dir/../pagespeed"
fi
SERVER_NAME=apache
source "$PAGESPEED_CODE_DIR/automatic/system_test_helpers.sh" || exit 1

DEFAULT_VCL="/etc/varnish/default.vcl"
BACKUP_DEFAULT_VCL=$TESTTMP"/default.vcl.bak"
DEBUG_CONF_VCL="$this_dir/debug_conf.v3.vcl"
TMP_DEBUG_CONF_VCL=$TESTTMP"/debug_conf.v3.vcl"

# Environment variables.
# MUST_BACKUP_DEFAULT_VCL is 1 by default because we would not like to overwrite
# the file without explicit permission.
: ${MUST_BACKUP_DEFAULT_VCL:=1}

# Helper method to print out varnish setup instructions.
print_varnish_setup_instructions() {
  echo "*** Please follow these instructions to install and start varnish"
  echo "*** on your system, so that apache_downstream_caching_test.sh can"
  echo "*** run successfully."
  echo "*** 1) sudo apt-get install varnish"
  echo "*** 2) sudo tee -a /etc/default/varnish <<EOF"
  echo "  DAEMON_OPTS=\"-a :8020 \ "
  echo "        -T localhost:6082 \ "
  echo "        -f /etc/varnish/default.vcl \ "
  echo "        -S /etc/varnish/secret \ "
  echo "        -s file,/var/lib/varnish/\$INSTANCE/varnish_storage.bin,1G\" "
  echo "EOF"
  echo "*** 3) sudo cp $DEBUG_CONF_VCL $DEFAULT_VCL"
  echo "*** 4) sudo service varnish restart"
  echo "*** 5) export VARNISH_SERVER=\"localhost:8020\""
  echo "*** 6) Rerun apache_downstream_caching_tests.sh"
}

OUT_CONTENTS_FILE="$OUTDIR/gzipped.html"
OUT_HEADERS_FILE="$OUTDIR/headers.html"
GZIP_WGET_ARGS="-q -S --header=Accept-Encoding:gzip -o $OUT_HEADERS_FILE -O - "

# Helper method that does a wget and verifies that the rewriting status matches
# the $1 argument that is passed to this method.
check_rewriting_status() {
  $WGET $WGET_ARGS $CACHABLE_HTML_LOC > $OUT_CONTENTS_FILE
  if $1; then
    check zgrep -q "pagespeed.ic" $OUT_CONTENTS_FILE
  else
    check_not zgrep -q "pagespeed.ic" $OUT_CONTENTS_FILE
  fi
  # Reset WGET_ARGS.
  WGET_ARGS=""
}

# Helper method that obtains a gzipped response and verifies that rewriting
# has happened. Also takes an extra parameter that identifies extra headers
# to be added during wget.
check_for_rewriting() {
  WGET_ARGS="$GZIP_WGET_ARGS $1"
  check_rewriting_status true
}

# Helper method that obtains a gzipped response and verifies that no rewriting
# has happened.
check_for_no_rewriting() {
  WGET_ARGS="$GZIP_WGET_ARGS"
  check_rewriting_status false
}

# Helper method to check that a variable in the statistics file has the expected
# value.
check_statistic_value() {
  check_from "$CURRENT_STATS" egrep -q "$1:[[:space:]]*$2"
}

check_num_downstream_cache_purge_attempts() {
  check_statistic_value $ATTEMPTS_VAR $1
}

check_num_successful_downstream_cache_purges() {
  check_statistic_value $SUCCESS_VAR $1
}

restore_default_vcl_from_backup() {
  sudo mv -f $BACKUP_DEFAULT_VCL $DEFAULT_VCL
  sudo service varnish restart
}

# Portions of the below test will be skipped if no VARNISH_SERVER is specified.
have_varnish_downstream_cache="1"
if [ -z ${VARNISH_SERVER:-} ]; then
  have_varnish_downstream_cache="0"
  echo "*** Skipping parts of the test because varnish server host:port has"
  echo "*** not been specified. If you'd like to run all parts of this test,"
  echo "*** please follow these instructions:"
  print_varnish_setup_instructions
  CACHABLE_HTML_HOST_PORT="http://${HOSTNAME}"
else
  CACHABLE_HTML_HOST_PORT="http://$VARNISH_SERVER"
  # Check for the presence of $DEFAULT_VCL file to confirm that varnish is
  # installed on the system. If varnish is not installed, print out
  # instructions for it.
  if [ ! -f $DEFAULT_VCL ]; then
    print_varnish_setup_instructions
    exit 1
  fi
  # Check whether the default.vcl being used by varnish is different from
  # debug_conf.v3.vcl.
  # a) If there are no differences, we assume that varnish has been restarted
  # after debug_conf.v3.vcl contents were copied over to default.vcl and
  # continue with the tests.
  cp -f $DEBUG_CONF_VCL $TMP_DEBUG_CONF_VCL
  if ! cmp -s $DEFAULT_VCL $TMP_DEBUG_CONF_VCL; then
    # Copy over the permissions and ownership attributes for $DEFAULT_VCL onto
    # $TMP_DEBUG_CONF_VCL.
    sudo chmod --reference=$DEFAULT_VCL $TMP_DEBUG_CONF_VCL
    sudo chown --reference=$DEFAULT_VCL $TMP_DEBUG_CONF_VCL
    if [ "$MUST_BACKUP_DEFAULT_VCL" = "1" ]; then
      # b) If there are differences, and MUST_BACKUP_DEFAULT_VCL is set to true,
      # we backup the default vcl.
      sudo mv $DEFAULT_VCL $BACKUP_DEFAULT_VCL
      trap restore_default_vcl_from_backup 0
    else
      # c) If there are differences, and MUST_BACKUP_DEFAULT_VCL is set to
      # false, we assume that the user would like to permanently copy over
      # debug_conf.v3.vcl into default.vcl for continuous testing purposes.
      echo "*** Overwriting /etc/varnish/default.vcl with the latest version"
      echo "*** of debug_conf.v3.vcl and restarting varnish."
      echo "*** You only need to do this once for every update to"
      echo "*** debug_conf.v3.vcl, which should not be very frequent."
    fi
    sudo cp -fp $TMP_DEBUG_CONF_VCL $DEFAULT_VCL
  fi
  # Restart varnish to clear its cache.
  sudo service varnish restart
fi

CACHABLE_HTML_LOC="$CACHABLE_HTML_HOST_PORT/mod_pagespeed_test"
CACHABLE_HTML_LOC+="/cachable_rewritten_html/downstream_caching.html"

STATS_URL="${HOSTNAME}/mod_pagespeed_statistics"
ATTEMPTS_VAR="downstream_cache_purge_attempts"
SUCCESS_VAR="successful_downstream_cache_purges"

# Number of downstream cache purges should be 0 here.
start_test Check that downstream cache purges are 0 initially.
CURRENT_STATS=$($WGET_DUMP $STATS_URL)
check_num_downstream_cache_purge_attempts 0
check_num_successful_downstream_cache_purges 0

# Output should not be rewritten and 1 successful purge should have
# occurred here.
start_test Check for case where rewritten cache should get purged.
check_for_no_rewriting
# Fetch until the purge happens.
fetch_until $STATS_URL "grep -c $ATTEMPTS_VAR:[[:space:]]*1" 1
if [ $have_varnish_downstream_cache = "1" ]; then
  CURRENT_STATS=$($WGET_DUMP $STATS_URL)
  check_num_successful_downstream_cache_purges 1
  check egrep -q "X-Cache: MISS" $OUT_HEADERS_FILE
  fi

# Output should be fully rewritten here.
start_test Check for case where rewritten cache should not get purged.
check_for_rewriting "--header=X-PSA-Blocking-Rewrite:psatest"
# Number of downstream cache purges should still be 1.
CURRENT_STATS=$($WGET_DUMP $STATS_URL)
check_num_downstream_cache_purge_attempts 1
if [ $have_varnish_downstream_cache = "1" ]; then
  check_num_successful_downstream_cache_purges 1
  check egrep -q "X-Cache: MISS" $OUT_HEADERS_FILE
fi

# Output should be fully rewritten here and we should have a HIT.
start_test Check for case when there should be a varnish cache hit.
check_for_rewriting ""
# Number of downstream cache purges should still be 1.
CURRENT_STATS=$($WGET_DUMP $STATS_URL)
check_num_downstream_cache_purge_attempts 1
if [ $have_varnish_downstream_cache = "1" ]; then
  check_num_successful_downstream_cache_purges 1
  check egrep -q "X-Cache: HIT" $OUT_HEADERS_FILE
fi

if [ $have_varnish_downstream_cache = "1" ]; then
  # Enable one of the beaconing dependent filters and verify interaction
  # between beaconing and downstream caching logic, by verifying that
  # whenever beaconing code is present in the rewritten page, the
  # output is also marked as a cache-miss, indicating that the instrumentation
  # was done by the backend.
  start_test Check whether beaconing is accompanied by a MISS always.
  WGET_ARGS="-S"
  CACHABLE_HTML_LOC+="?ModPagespeedFilters=lazyload_images"
  fetch_until -gzip $CACHABLE_HTML_LOC \
      "zgrep -c \"pagespeed\.CriticalImages\.Run\"" 1
  check fgrep -q 'X-Cache: MISS' $WGET_OUTPUT
  check fgrep -q 'Cache-Control: no-cache, max-age=0' $WGET_OUTPUT
fi


check_failures_and_exit
