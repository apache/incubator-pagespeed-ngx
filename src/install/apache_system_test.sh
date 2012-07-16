#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Runs all Apache-specific and general system tests.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Expects APACHE_DEBUG_PAGESPEED_CONF to point to our config file,
# APACHE_LOG to the log file

if [ -z $APACHE_DEBUG_PAGESPEED_CONF ]; then
  APACHE_DEBUG_PAGESPEED_CONF=/usr/local/apache2/conf/pagespeed.conf
fi

if [ -z $APACHE_LOG ]; then
  APACHE_LOG=/usr/local/apache2/logs/error_log
fi

if [ -z $APACHE_DOC_ROOT ]; then
  APACHE_DOC_ROOT=/usr/local/apache2/htdocs/
fi

# Run General system tests.
this_dir=$(dirname $0)
source $this_dir/system_test.sh

rm -rf $OUTDIR
mkdir -p $OUTDIR

# General system tests

echo Checking for correct default X-Mod-Pagespeed header format.
$WGET_DUMP $EXAMPLE_ROOT/combine_css.html | \
  check egrep -q '^X-Mod-Pagespeed: [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+-[0-9]+'

echo TEST: mod_pagespeed is running in Apache and writes the expected header.
echo $WGET_DUMP $EXAMPLE_ROOT/combine_css.html
HTML_HEADERS=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)

echo TEST: mod_pagespeed is defaulting to more than PassThrough
# Note: this is relying on lack of .htaccess in mod_pagespeed_test
check [ ! -f $APACHE_DOC_ROOT/mod_pagespeed_test/.htaccess ]
fetch_until $TEST_ROOT/bot_test.html 'grep -c \.pagespeed\.' 2

# Determine whether statistics are enabled or not.  If not, don't test them,
# but do an additional regression test that tries harder to get a cache miss.
if fgrep -q "# ModPagespeedStatistics off" $APACHE_DEBUG_PAGESPEED_CONF; then
  echo TEST: 404s are served and properly recorded.
  NUM_404=$($WGET_DUMP $STATISTICS_URL | grep resource_404_count | cut -d: -f2)
  NUM_404=$(($NUM_404+1))
  FETCHED=$OUTDIR/stats
  $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1 \
    | check fgrep -q "404 Not Found"
  $WGET_DUMP $STATISTICS_URL > $FETCHED
  check egrep -q "^resource_404_count: *$NUM_404$" $FETCHED
else
  echo TEST: 404s are served.  Statistics are disabled so not checking them.
  $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1 \
    | check fgrep -q "404 Not Found"

  echo TEST: 404s properly on uncached invalid resource.
  $WGET -O /dev/null $BAD_RND_RESOURCE_URL 2>&1 \
    | check fgrep -q "404 Not Found"
fi

# Test /mod_pagespeed_message exists.
echo TEST: Check if /mod_pagespeed_message page exists.
$WGET --save-headers -q -O - $MESSAGE_URL | head -1 \
  | check fgrep "HTTP/1.1 200 OK"

# Note: There is a similar test in system_test.sh
#
# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?ModPagespeed=off.
echo TEST: directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q "$EXAMPLE_ROOT/?ModPagespeed=off" \
  -O $OUTDIR/mod_pagespeed_example
check $WGET -q "$EXAMPLE_ROOT/index.html?ModPagespeed=off" -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example


# Individual filter tests, in alphabetical order

# This is dependent upon having a /mod_pagespeed_beacon handler.
test_filter add_instrumentation beacons load.
check $WGET_PREREQ http://$HOSTNAME/mod_pagespeed_beacon?ets=load:13
check fgrep -q "204 No Content" $WGET_OUTPUT

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'grep -c text/css' 1
check $WGET_PREREQ $URL
test_resource_ext_corruption $URL $combine_css_filename

test_filter extend_cache rewrites an image tag.
fetch_until $URL 'grep -c src.*91_WewrLtP' 1
check $WGET_PREREQ $WGET_ARGS $URL
echo about to test resource ext corruption...
test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined
echo TEST: compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
echo JS_HEADERS=$JS_HEADERS
echo $JS_HEADERS | check egrep -qi 'HTTP/1[.]. 200 OK'
echo $JS_HEADERS | check fgrep -qi 'Content-Encoding: gzip'
echo $JS_HEADERS | check fgrep -qi 'Vary: Accept-Encoding'
echo $JS_HEADERS | check fgrep -qi 'Etag: W/0'
echo $JS_HEADERS | check fgrep -qi 'Last-Modified:'

# Test RetainComment directive.
test_filter remove_comments retains appropriate comments.
check $WGET_PREREQ $URL
check grep -q retained $FETCHED        # RetainComment directive

# TODO(sligocki): This test needs to be run before below tests.
# Remove once below tests are moved to system_test.sh.
test_filter rewrite_images inlines, compresses, and resizes.
fetch_until $URL 'grep -c data:image/png' 1  # inlined
fetch_until $URL 'grep -c .pagespeed.ic' 2   # other 2 images optimized
check run_wget_with_args $URL
check [ "$(stat -c %s $OUTDIR/xBikeCrashIcn*)" -lt 25000 ]      # re-encoded
check [ "$(stat -c %s $OUTDIR/*256x192*Puzzle*)"  -lt 24126  ]  # resized
URL=$EXAMPLE_ROOT"/rewrite_images.html?ModPagespeedFilters=rewrite_images"
IMG_URL=$(egrep -o http://.*.pagespeed.*.jpg $FETCHED | head -n1)
check [ x"$IMG_URL" != x ]
echo TEST: headers for rewritten image "$IMG_URL"
IMG_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $IMG_URL 2>&1)
echo "IMG_HEADERS=\"$IMG_HEADERS\""
echo $IMG_HEADERS | check egrep -qi 'HTTP/1[.]. 200 OK'
# Make sure we have some valid headers.
echo "$IMG_HEADERS" | check fgrep -qi 'Content-Type: image/jpeg'
# Make sure the response was not gzipped.
echo TEST: Images are not gzipped.
echo "$IMG_HEADERS" | check_not fgrep -i 'Content-Encoding: gzip'
# Make sure there is no vary-encoding
echo TEST: Vary is not set for images.
echo "$IMG_HEADERS" | check_not fgrep -i 'Vary: Accept-Encoding'
# Make sure there is an etag
echo TEST: Etags is present.
echo "$IMG_HEADERS" | check fgrep -qi 'Etag: W/0'
# Make sure an extra header is propagated from input resource to output
# resource.  X-Extra-Header is added in debug.conf.template.
echo TEST: Extra header is present
echo "$IMG_HEADERS" | check fgrep -qi 'X-Extra-Header'
# Make sure there is a last-modified tag
echo TEST: Last-modified is present.
echo "$IMG_HEADERS" | check fgrep -qi 'Last-Modified'

# Depends upon "Header append Vary User-Agent" and ModPagespeedRespectVary.
echo TEST: respect vary user-agent
URL=$TEST_ROOT/vary/index.html?ModPagespeedFilters=inline_css
echo $WGET_DUMP $URL
$WGET_DUMP $URL | check_not fgrep "<style>"

echo TEST: ModPagespeedShardDomain directive in .htaccess file
rm -rf $OUTDIR
mkdir $OUTDIR
echo $WGET_DUMP $TEST_ROOT/shard/shard.html
fetch_until $TEST_ROOT/shard/shard.html 'grep -c \.pagespeed\.' 4
check $WGET_DUMP $TEST_ROOT/shard/shard.html > $OUTDIR/shard.out.html
check [ $(grep -ce href=\"http://shard1 $OUTDIR/shard.out.html) = 2 ];
check [ $(grep -ce href=\"http://shard2 $OUTDIR/shard.out.html) = 2 ];

echo TEST: server-side includes
rm -rf $OUTDIR
mkdir $OUTDIR
echo $WGET_DUMP $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css
fetch_until $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css \
    'grep -c \.pagespeed\.' 1
check $WGET_DUMP $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css \
  > $OUTDIR/ssi.out.html
check [ $(grep -ce $combine_css_filename $OUTDIR/ssi.out.html) = 1 ];

echo TEST: mod_rewrite
echo $WGET_DUMP $TEST_ROOT/redirect/php/
check $WGET_DUMP $TEST_ROOT/redirect/php/ > $OUTDIR/redirect_php.html
check \
  [ $(grep -ce "href=\"/mod_pagespeed_test/" $OUTDIR/redirect_php.html) = 2 ];

echo TEST: ModPagespeedLoadFromFile
URL=$TEST_ROOT/load_from_file/index.html?ModPagespeedFilters=inline_css
fetch_until $URL 'grep -c blue' 1

echo TEST: Custom headers remain on HTML, but cache should be disabled.
URL=$TEST_ROOT/rewrite_compressed_js.html
echo $WGET_DUMP $URL
HTML_HEADERS=$($WGET_DUMP $URL)
echo $HTML_HEADERS | check egrep -q "X-Extra-Header: 1"
# The extra header should only be added once, not twice.
echo $HTML_HEADERS | check egrep -q -v "X-Extra-Header: 1, 1"
echo $HTML_HEADERS | check egrep -q 'Cache-Control: max-age=0, no-cache'

echo TEST: Custom headers remain on resources, but cache should be 1 year.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
echo $WGET_DUMP $URL
RESOURCE_HEADERS=$($WGET_DUMP $URL)
echo $RESOURCE_HEADERS | check egrep -q 'X-Extra-Header: 1'
# The extra header should only be added once, not twice.
echo $RESOURCE_HEADERS | check egrep -q -v 'X-Extra-Header: 1, 1'
echo $RESOURCE_HEADERS | check egrep -q 'Cache-Control: max-age=31536000'

echo TEST: ModPagespeedModifyCachingHeaders
URL=$TEST_ROOT/retain_cache_control/index.html
$WGET_DUMP $URL
$WGET_DUMP $URL | check grep -q "Cache-Control: private, max-age=3000"

test_filter combine_javascript combines 2 JS files into 1.
echo TEST: combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?ModPagespeedFilters=combine_javascript
fetch_until $URL 'grep -c src=' 4

echo TEST: aris disables js combining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 2

echo TEST: aris disables js combining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 1

test_filter inline_javascript inlines a small JS file
echo TEST: aris disables js inlining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 1

echo TEST: aris disables js inlining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 0

test_filter rewrite_javascript minifies JavaScript and saves bytes.
echo TEST: aris disables js cache extention for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=rewrite_javascript"
# first check something that should get rewritten to know we're done with
# rewriting
fetch_until $URL 'grep -c "src=\"../normal.js\""' 0
check [ $($WGET_DUMP $URL | grep -c "src=\"../introspection.js\"") = 1 ]

echo TEST: aris disables js cache extension only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=rewrite_javascript"
fetch_until $URL 'grep -c src=\"normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"introspection.js\") = 0 ]

# Check that no filter changes urls for introspective javascript if
# avoid_renaming_introspective_javascript is on
echo TEST: aris disables url modification for introspective js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=testing,core"
# first check something that should get rewritten to know we're done with
# rewriting
fetch_until $URL 'grep -c src=\"../normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"../introspection.js\") = 1 ]

echo TEST: aris disables url modification only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=testing,core"
fetch_until $URL 'grep -c src=\"normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"introspection.js\") = 0 ]

echo TEST: HTML add_instrumentation lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?ModPagespeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]

echo TEST: XHTML add_instrumentation also lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.xhtml\
?ModPagespeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]

# TODO(sligocki): TEST: ModPagespeedMaxSegmentLength

if [ "$CACHE_FLUSH_TEST" == "on" ]; then
  WGET_ARGS=""

  SECONDARY_HOSTNAME=$(echo $HOSTNAME | sed -e s/8080/$APACHE_SECONDARY_PORT/g)
  if [ "$SECONDARY_HOSTNAME" == "$HOSTNAME" ]; then
    SECONDARY_HOSTNAME=${HOSTNAME}:$APACHE_SECONDARY_PORT
  fi
  SECONDARY_TEST_ROOT=http://$SECONDARY_HOSTNAME/mod_pagespeed_test

  echo TEST: add_instrumentation has added unload handler with \
      ModPagespeedReportUnloadTime enabled in APACHE_SECONDARY_PORT.
  $WGET -O $WGET_OUTPUT \
      $SECONDARY_TEST_ROOT/add_instrumentation.html\
?ModPagespeedFilters=add_instrumentation
  check [ $(grep -c "<script" $WGET_OUTPUT) = 3 ]
  check [ $(grep -c 'ets=unload' $WGET_OUTPUT) = 1 ]

  echo TEST: Cache flushing works by touching cache.flush in cache directory.

  echo Clear out our existing state before we begin the test.
  echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush

  URL_PATH=cache_flush_test.html?ModPagespeedFilters=inline_css
  URL=$TEST_ROOT/$URL_PATH
  CSS_FILE=$APACHE_DOC_ROOT/mod_pagespeed_test/update.css
  TMP_CSS_FILE=/tmp/update.css.$$

  # First, write 'blue' into the css file and make sure it gets inlined into
  # the html.
  echo "echo \".class myclass { color: blue; }\" > $CSS_FILE"
  echo ".class myclass { color: blue; }" >$TMP_CSS_FILE
  $SUDO cp $TMP_CSS_FILE $CSS_FILE
  fetch_until $URL 'grep -c blue' 1

  # Also do the same experiment using a different VirtualHost.  It points
  # to the same htdocs, but uses a separate cache directory.
  SECONDARY_URL=$SECONDARY_TEST_ROOT/$URL_PATH
  fetch_until $SECONDARY_URL 'grep -c blue' 1

  # Track how many flushes were noticed by Apache processes up till
  # this point in time.  Note that each Apache process/vhost
  # separately detects the 'flush'.
  NUM_INITIAL_FLUSHES=$($WGET_DUMP $STATISTICS_URL | grep cache_flush_count \
    | cut -d: -f2)

  # Now change the file to 'green'.
  echo echo ".class myclass { color: green; }" ">" $CSS_FILE
  echo ".class myclass { color: green; }" >$TMP_CSS_FILE
  $SUDO cp $TMP_CSS_FILE $CSS_FILE

  # We might have stale cache for 5 minutes, so the result might stay
  # 'blue', but we can't really test for that since the child process
  # handling this request might not have it in cache.
  # fetch_until $URL 'grep -c blue' 1

  # Flush the cache by touching a special file in the cache directory.  Now
  # css gets re-read and we get 'green' in the output.  Sleep here to avoid
  # a race due to 1-second granularity of file-system timestamp checks.  For
  # the test to pass we need to see time pass from the previous 'touch'.
  sleep 2
  echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  fetch_until $URL 'grep -c green' 1

  NUM_FLUSHES=$($WGET_DUMP $STATISTICS_URL | grep cache_flush_count \
    | cut -d: -f2)
  NUM_NEW_FLUSHES=$(expr $NUM_FLUSHES - $NUM_INITIAL_FLUSHES)
  echo NUM_NEW_FLUSHES = $NUM_NEW_FLUSHES
  check [ $NUM_NEW_FLUSHES -ge 1 ]
  check [ $NUM_NEW_FLUSHES -lt 20 ]

  # However, the secondary cache might not have sees this cache-flush, but
  # due to the multiple child processes, each of which does polling separately,
  # we cannot guarantee it.  I think if we knew we were running a 'worker' mpm
  # with just 1 child process we could do this test.
  # fetch_until $SECONDARY_URL 'grep -c blue' 1

  # Now flush the secondary cache too so it can see the change to 'green'.
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  fetch_until $SECONDARY_URL 'grep -c green' 1

  # Clean up update.css from mod_pagespeed_test so it doesn't leave behind
  # a stray file not under source control.
  echo $SUDO rm -f $CSS_FILE
  $SUDO rm -f $CSS_FILE
  rm -f $TMP_CSS_FILE

  # connection_refused.html references modpagespeed.com:1023/someimage.png.
  # mod_pagespeed will attempt to connect to that host and port to fetch the
  # input resource using serf.  We expect the connection to be refused.  Relies
  # on "ModPagespeedDomain modpagespeed.com:1023" in debug.conf.template.  Also
  # relies on running after a cache-flush to avoid bypassing the serf fetch,
  # since mod_pagespeed remembers fetch-failures in its cache for 5 minutes.
  echo TEST: Connection refused handling

  # Monitor the Apache log starting now.  tail -F will catch log rotations.
  SERF_REFUSED_PATH=/tmp/instaweb_apache_serf_refused.$$
  echo APACHE_LOG = $APACHE_LOG
  tail --sleep-interval=0.1 -F $APACHE_LOG > $SERF_REFUSED_PATH &
  TAIL_PID=$!
  # Actually kick off the request.
  echo $WGET_DUMP $TEST_ROOT/connection_refused.html
  echo checking...
  check $WGET_DUMP $TEST_ROOT/connection_refused.html > /dev/null
  echo check done
  # If we are spewing errors, this gives time to spew lots of them.
  sleep 1
  # Wait up to 10 seconds for the background fetch of someimage.png to fail.
  for i in {1..100}; do
    ERRS=$(grep -c "Serf status 111" $SERF_REFUSED_PATH)
    if [ $ERRS -ge 1 ]; then
      break;
    fi;
    /bin/echo -n "."
    sleep 0.1
  done;
  /bin/echo "."
  # Kill the log monitor silently.
  kill $TAIL_PID
  wait $TAIL_PID 2> /dev/null
  check [ $ERRS -ge 1 ]
  # Make sure we have the URL detail we expect because
  # ModPagespeedListOutstandingUrlsOnError is on in debug.conf.template.
  echo Check that ModPagespeedSerfListOutstandingUrlsOnError works
  check grep "URL http://modpagespeed.com:1023/someimage.png active for " \
      $SERF_REFUSED_PATH

  echo "TEST: Blocking rewrite enabled."
  # We assume that blocking_rewrite_test_dont_reuse_1.jpg will not be rewritten
  # on the first request since it takes significantly more time to rewrite than
  # the rewrite deadline and it is not already accessed by another request
  # earlier.
  BLOCKING_REWRITE_URL="$TEST_ROOT/blocking_rewrite.html?\
ModPagespeedFilters=rewrite_images"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  OLDSTATS=$OUTDIR/blocking_rewrite_stats.old
  NEWSTATS=$OUTDIR/blocking_rewrite_stats.new
  $WGET_DUMP $STATISTICS_URL > $OLDSTATS
  $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $BLOCKING_REWRITE_URL \
    > $OUTFILE
  check [ $? = 0 ]
  $WGET_DUMP $STATISTICS_URL > $NEWSTATS
  check_stat $OLDSTATS $NEWSTATS image_rewrites 1
  check_stat $OLDSTATS $NEWSTATS cache_hits 0
  check_stat $OLDSTATS $NEWSTATS cache_misses 1
  check_stat $OLDSTATS $NEWSTATS cache_inserts 2
  check_stat $OLDSTATS $NEWSTATS num_rewrites_executed 1

  echo "TEST: Blocking rewrite enabled using wrong key."
  BLOCKING_REWRITE_URL="$SECONDARY_TEST_ROOT/\
blocking_rewrite_another.html?ModPagespeedFilters=rewrite_images"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: junk' \
    $BLOCKING_REWRITE_URL > $OUTFILE
  check [ $? = 0 ]
  check [ $(grep -c "[.]pagespeed[.]" $OUTFILE) -lt 1 ]

  fetch_until $BLOCKING_REWRITE_URL 'grep -c [.]pagespeed[.]' 1
fi

# Cleanup
rm -rf $OUTDIR
echo "PASS."
