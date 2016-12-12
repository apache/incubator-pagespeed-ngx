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
start_test Cache flushing works by touching cache.flush in cache directory.

# If we write fixed values into the css file here, there is a risk that
# we will end up seeing the 'right' value because an old process hasn't
# invalidated things yet, rather than because it updated to what we expect
# in the first run followed by what we expect in the second run.
# So, we incorporate the timestamp into RGB colors, using hours
# prefixed with 1 (as 0-123 fits the 0-255 range) to get a second value.
# A one-second precision is good enough since there is a sleep 2 below.
COLOR_SUFFIX=`date +%H,%M,%S\)`
COLOR0=rgb\($COLOR_SUFFIX
COLOR1=rgb\(1$COLOR_SUFFIX

echo Clear out our existing state before we begin the test.
for flushfile in $MOD_PAGESPEED_CACHE/cache.flush \
    ${MOD_PAGESPEED_CACHE}_secondary/cache.flush \
    ${MOD_PAGESPEED_CACHE}_ipro_for_browser/cache.flush; do
  echo $SUDO touch $flushfile
  $SUDO touch $flushfile
  $SUDO chcon --reference=${APACHE_DOC_ROOT} $flushfile || true
done
sleep 1

CACHE_TESTING_DIR="$APACHE_DOC_ROOT/cache_flush"
CACHE_TESTING_TMPDIR="$CACHE_TESTING_DIR/$$"
echo $SUDO mkdir "$CACHE_TESTING_TMPDIR"
$SUDO mkdir "$CACHE_TESTING_TMPDIR"
echo $SUDO cp "$CACHE_TESTING_DIR/cache_flush_test.html"\
              "$CACHE_TESTING_TMPDIR/"
$SUDO cp "$CACHE_TESTING_DIR/cache_flush_test.html" "$CACHE_TESTING_TMPDIR/"
CSS_FILE="$CACHE_TESTING_TMPDIR/update.css"
URL_PATH=cache_flush/$$/cache_flush_test.html
URL="$PRIMARY_SERVER/$URL_PATH"
TMP_CSS_FILE=$TESTTMP/update.css

# First, write color 0 into the css file and make sure it gets inlined into
# the html.
echo "echo \".class myclass { color: $COLOR0; }\" > $CSS_FILE"
echo ".class myclass { color: $COLOR0; }" >$TMP_CSS_FILE
chmod a+r $TMP_CSS_FILE  # in case the user's umask doesn't allow o+r
$SUDO cp $TMP_CSS_FILE $CSS_FILE
fetch_until $URL "grep -c $COLOR0" 1

# Also do the same experiment using a different VirtualHost.  It points
# to the same htdocs, but uses a separate cache directory.
SECONDARY_URL="$SECONDARY_ROOT/$URL_PATH"
http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR0" 1

# Track how many flushes were noticed by Apache processes up till
# this point in time.  Note that each Apache process/vhost
# separately detects the 'flush'.
NUM_INITIAL_FLUSHES=$(scrape_stat cache_flush_count)

# Now change the file to $COLOR1.
echo echo ".class myclass { color: $COLOR1; }" ">" $CSS_FILE
echo ".class myclass { color: $COLOR1; }" >$TMP_CSS_FILE
chmod a+r $TMP_CSS_FILE  # in case the user's umask doesn't allow o+r
$SUDO cp $TMP_CSS_FILE $CSS_FILE

# We might have stale cache for 5 seconds, so the result might stay
# $COLOR0, but we can't really test for that since the child process
# handling this request might not have it in cache.
# fetch_until $URL 'grep -c $COLOR0' 1

# Flush the cache by touching a special file in the cache directory.  Now
# css gets re-read and we get $COLOR1 in the output.  Sleep here to avoid
# a race due to 1-second granularity of file-system timestamp checks.  For
# the test to pass we need to see time pass from the previous 'touch'.
sleep 2
echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
$SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
sleep 1
fetch_until $URL "grep -c $COLOR1" 1

# TODO(jmarantz): we can change this test to be more exacting now, since
# to address Issue 568, we should only get one cache-flush bump every time
# we touch the file.
if [ $statistics_enabled -ne 0 ]; then
  NUM_FLUSHES=$(scrape_stat cache_flush_count)
  NUM_NEW_FLUSHES=$(expr $NUM_FLUSHES - $NUM_INITIAL_FLUSHES)
  echo NUM_NEW_FLUSHES = $NUM_FLUSHES - \
    $NUM_INITIAL_FLUSHES = $NUM_NEW_FLUSHES
  check [ $NUM_NEW_FLUSHES -ge 1 ]
  check [ $NUM_NEW_FLUSHES -lt 20 ]
fi

# However, the secondary cache might not have seen this cache-flush, but
# due to the multiple child processes, each of which does polling separately,
# we cannot guarantee it.  I think if we knew we were running a 'worker' mpm
# with just 1 child process we could do this test.
# fetch_until $SECONDARY_URL 'grep -c blue' 1

# Now flush the secondary cache too so it can see the change to $COLOR1.
echo $SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
$SUDO touch ${MOD_PAGESPEED_CACHE}_secondary/cache.flush
sleep 1
http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR1" 1

# Clean up so we don't leave behind a stray file not under source control.
echo $SUDO rm -f $CACHE_TESTING_TMPDIR
$SUDO rm -rf "$CACHE_TESTING_TMPDIR"
rm -f $TMP_CSS_FILE

# https://github.com/pagespeed/mod_pagespeed/issues/1077
start_test Cache purging with PageSpeed off in vhost, but on in htacess file.
cache_purge_test http://psoff-htaccess-on.example.com

# Run a simple cache_purge test but in a vhost with ModPagespeed off, and
# a subdirectory with htaccess file turning it back on, addressing
# https://github.com/pagespeed/mod_pagespeed/issues/1077
#
# TODO(jefftk): delete this from here and uncomment the same test in
# system/system_test.sh once nginx_system_test suppressions &/or
# "pagespeed off;" in server block allow location-overrides in ngx_pagespeed.
# See https://github.com/pagespeed/ngx_pagespeed/issues/968
start_test Cache purging with PageSpeed off in vhost, but on in directory.
cache_purge_test http://psoff-dir-on.example.com
