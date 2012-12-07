#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Runs all Apache-specific and general system tests.
#
# See system_test_helpers.sh for usage.
#
# Expects APACHE_DEBUG_PAGESPEED_CONF to point to our config file,
# APACHE_LOG to the log file.
#
# CACHE_FLUSH_TEST=on can be passed to test our cache.flush behavior
# NO_VHOST_MERGE=on can be passed to tell tests to assume
# that ModPagespeedInheritVHostConfig has been turned off.

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

# Run General system tests.
this_dir=$(dirname $0)
source "$this_dir/system_test.sh" || exit 1

# Extract secondary hostname when set. Currently it's only set
# when doing the cache flush test, but it can be used in other
# tests we run in that run.
if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  SECONDARY_HOSTNAME=$(echo $HOSTNAME | sed -e s/8080/$APACHE_SECONDARY_PORT/g)
  if [ "$SECONDARY_HOSTNAME" = "$HOSTNAME" ]; then
    SECONDARY_HOSTNAME=${HOSTNAME}:$APACHE_SECONDARY_PORT
  fi

  # To fetch from the secondary test root, we must set
  # http_proxy=$(SECONDARY_HOSTNAME) during fetches.
  SECONDARY_TEST_ROOT=http://secondary.example.com/mod_pagespeed_test
fi

rm -rf $OUTDIR
mkdir -p $OUTDIR

# Grab a timestamp now so that we can check that logging works.
# Also determine where the log file is.
if egrep -q "^    # ModPagespeedStatistics off$" $APACHE_DEBUG_PAGESPEED_CONF &&
   egrep -q "^ ModPagespeedStatisticsLogging on$" $APACHE_DEBUG_PAGESPEED_CONF;
   then
  MOD_PAGESPEED_STATS_LOG=$(sed -n 's/^ ModPagespeedStatisticsLoggingFile //p' \
      $APACHE_DEBUG_PAGESPEED_CONF)
  MOD_PAGESPEED_STATS_LOG=$(echo $MOD_PAGESPEED_STATS_LOG | sed -n 's/\"//gp')
  # Wipe the logs so we get a clean start.
  rm $MOD_PAGESPEED_STATS_LOG*
  START_TIME=$(date +%s)000 # We need this in milliseconds.
  sleep 2; # Make sure we're around long enough to log stats.
fi

# General system tests

start_test Check for correct default X-Mod-Pagespeed header format.
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q \
  '^X-Mod-Pagespeed: [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+-[0-9]+'

start_test mod_pagespeed is running in Apache and writes the expected header.
echo $WGET_DUMP $EXAMPLE_ROOT/combine_css.html
HTML_HEADERS=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)

start_test mod_pagespeed is defaulting to more than PassThrough
# Note: this is relying on lack of .htaccess in mod_pagespeed_test
check [ ! -f $APACHE_DOC_ROOT/mod_pagespeed_test/.htaccess ]
fetch_until $TEST_ROOT/bot_test.html 'grep -c \.pagespeed\.' 2

# Determine whether statistics are enabled or not.  If not, don't test them,
# but do an additional regression test that tries harder to get a cache miss.
if fgrep -q "# ModPagespeedStatistics off" $APACHE_DEBUG_PAGESPEED_CONF; then
  start_test 404s are served and properly recorded.
  NUM_404=$(scrape_stat resource_404_count)
  WGET_ERROR=$($WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
  check_from "$WGET_ERROR" fgrep -q "404 Not Found"

  # Check that the stat got bumped.
  check [ $(expr $(scrape_stat resource_404_count) - $NUM_404) -eq 1 ]

  start_test Non-local access to statistics fails.
  MACHINE_NAME=$(hostname)
  ALT_STAT_URL=$(echo $STATISTICS_URL | sed s#localhost#$MACHINE_NAME#)

  wget $ALT_STAT_URL >& "$TEMPDIR/alt_stat_url.$$"
  check [ $? = 8 ]
  rm -f "$TEMPDIR/alt_stat_url.$$"


else
  start_test 404s are served.  Statistics are disabled so not checking them.
  OUT=$($WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"

  start_test 404s properly on uncached invalid resource.
  OUT=$($WGET -O /dev/null $BAD_RND_RESOURCE_URL 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"
fi


# Test /mod_pagespeed_message exists.
start_test Check if /mod_pagespeed_message page exists.
OUT=$($WGET --save-headers -q -O - $MESSAGE_URL | head -1)
check_from "$OUT" fgrep "HTTP/1.1 200 OK"

# Note: There is a similar test in system_test.sh
#
# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?ModPagespeed=off.
start_test directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q "$EXAMPLE_ROOT/?ModPagespeed=off" \
  -O $OUTDIR/mod_pagespeed_example
check $WGET -q "$EXAMPLE_ROOT/index.html?ModPagespeed=off" -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example

start_test Request Headers affect MPS options

# Get the special file response_headers.html and test the result.
# This file has Apache request_t headers_out and err_headers_out modified
# according to the query parameter.  The modification happens in
# instaweb_handler when the handler == kGenerateResponseWithOptionsHandler.
# Possible query flags include: headers_out, headers_errout, headers_override,
# and headers_combine.
function response_header_test() {
    query=$1
    mps_on=$2
    comments_removed=$3
    rm -rf $OUTDIR
    mkdir -p $OUTDIR

    # Get the file
    check $WGET -q -S -O - \
      "$TEST_ROOT/response_headers.html?$query" >& $OUTDIR/header_out

    # Make sure that any MPS option headers were stripped
    check_not grep -q ^ModPagespeed: $OUTDIR/header_out

    # Verify if MPS is on or off
    if [ $mps_on = "no" ]; then
      # Verify that ModPagespeed was off
      check_not fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check_not fgrep -q '<script' $OUTDIR/header_out
    else
      # Verify that ModPagespeed was on
      check fgrep -q 'X-Mod-Pagespeed:' $OUTDIR/header_out
      check fgrep -q '<script' $OUTDIR/header_out
    fi

    # Verify if comments were stripped
    if [ $comments_removed = "no" ]; then
      # Verify that comments were not removed
      check fgrep -q '<!--' $OUTDIR/header_out
    else
      # Verify that comments were removed
      check_not fgrep -q '<!--' $OUTDIR/header_out
    fi
}

# headers_out =     MPS: off
# err_headers_out =
response_header_test headers_out no no

# headers_out =
# err_headers_out = MPS: on
response_header_test headers_errout no no

# Note: The next two tests will break if remove_comments gets into the
# CoreFilter set.

# headers_out     = MPS: off, Filters: -remove_comments
# err_headers_out = MPS: on,  Filters: +remove_comments
# err_headers should is processed after headers_out, and so it should override
# but disabling a filter trumps enabling one. The overriding is described in
# the code for build_context_for_request.
response_header_test headers_override yes no

# headers_out     = MPS: on
# err_headers_out = Filters: +remove_comments
response_header_test headers_combine yes yes

start_test Respect X-Forwarded-Proto when told to
FETCHED=$OUTDIR/x_forwarded_proto
URL=$TEST_ROOT/?ModPagespeedFilters=add_base_tag
check $WGET_DUMP -O $FETCHED --header="X-Forwarded-Proto: https" $URL
# When enabled, we respect X-Forwarded-Proto and thus list base as https.
check fgrep -q '<base href="https://' $FETCHED


# Individual filter tests, in alphabetical order

# This is dependent upon having a /mod_pagespeed_beacon handler.
test_filter add_instrumentation beacons load.
check run_wget_with_args http://$HOSTNAME/mod_pagespeed_beacon?ets=load:13
check fgrep -q "204 No Content" $WGET_OUTPUT

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'grep -c text/css' 1
check run_wget_with_args $URL
test_resource_ext_corruption $URL $combine_css_filename

test_filter extend_cache rewrites an image tag.
fetch_until $URL 'grep -c src.*91_WewrLtP' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined
start_test compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
echo JS_HEADERS=$JS_HEADERS
check_from "$JS_HEADERS" egrep -qi 'HTTP/1[.]. 200 OK'
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
check_from "$JS_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'

# Test RetainComment directive.
test_filter remove_comments retains appropriate comments.
check run_wget_with_args $URL
check grep -q retained $FETCHED        # RetainComment directive

# Make sure that when in PreserveURLs mode that we don't rewrite URLs. This is
# non-exhaustive, the unit tests should cover the rest.
# Note: We block with psatest here because this is a negative test.  We wouldn't
# otherwise know how many wget attempts should be made.
WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
start_test PreserveURLs on prevents URL rewriting
FILE=preserveurls/on/preserveurls.html
URL=$TEST_ROOT/$FILE
FETCHED=$OUTDIR/preserveurls.html
check run_wget_with_args $URL
unset WGET_ARGS
check_not_from "$FETCHED" fgrep -q \.pagespeed\.

# When PreserveURLs is off do a quick check to make sure that normal rewriting
# occurs.  This is not exhaustive, the unit tests should cover the rest.
start_test PreserveURLs off causes URL rewriting
FILE=preserveurls/off/preserveurls.html
URL=$TEST_ROOT/$FILE
FETCHED=$OUTDIR/preserveurls.html
# Check that style.css was inlined.
fetch_until $URL 'grep -c #9370db' 1
# Check that introspection.js was inlined.
fetch_until $URL 'grep -c script_tags' 1
# Check that the image was optimized.
fetch_until $URL 'grep -c BikeCrashIcn\.png\.pagespeed\.' 2

# TODO(jkarlin): When ajax rewriting is in MPS check that it works with
# MPS.


# TODO(sligocki): This test needs to be run before below tests.
# Remove once below tests are moved to system_test.sh.
test_filter rewrite_images [apache_test] inlines, compresses, and resizes.
fetch_until $URL 'grep -c data:image/png' 1  # inlined
fetch_until $URL 'grep -c .pagespeed.ic' 2   # two images optimized

# Verify with a blocking fetch that pagespeed_no_transform worked and was
# stripped.
fetch_until $URL 'grep -c "images/disclosure_open_plus.png"' 1 \
  '--header=X-PSA-Blocking-Rewrite:psatest'
fetch_until $URL 'grep -c "pagespeed_no_transform"' 0 \
  '--header=X-PSA-Blocking-Rewrite:psatest'

check run_wget_with_args $URL
check_file_size "$OUTDIR/xBikeCrashIcn*" -lt 25000     # re-encoded
check_file_size "$OUTDIR/*256x192*Puzzle*" -lt 24126   # resized
URL=$EXAMPLE_ROOT"/rewrite_images.html?ModPagespeedFilters=rewrite_images"
IMG_URL=$(egrep -o http://.*.pagespeed.*.jpg $FETCHED | head -n1)
check [ x"$IMG_URL" != x ]
start_test headers for rewritten image
echo IMG_URL="$IMG_URL"
IMG_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $IMG_URL 2>&1)
echo "IMG_HEADERS=\"$IMG_HEADERS\""
check_from "$IMG_HEADERS" egrep -qi 'HTTP/1[.]. 200 OK'
# Make sure we have some valid headers.
check_from "$IMG_HEADERS" fgrep -qi 'Content-Type: image/jpeg'
# Make sure the response was not gzipped.
start_test Images are not gzipped.
check_not_from "$IMG_HEADERS" fgrep -i 'Content-Encoding: gzip'
# Make sure there is no vary-encoding
start_test Vary is not set for images.
check_not_from "$IMG_HEADERS" fgrep -i 'Vary: Accept-Encoding'
# Make sure there is an etag
start_test Etags is present.
check_from "$IMG_HEADERS" fgrep -qi 'Etag: W/"0"'
# Make sure an extra header is propagated from input resource to output
# resource.  X-Extra-Header is added in debug.conf.template.
start_test Extra header is present
check_from "$IMG_HEADERS" fgrep -qi 'X-Extra-Header'
# Make sure there is a last-modified tag
start_test Last-modified is present.
check_from "$IMG_HEADERS" fgrep -qi 'Last-Modified'

IMAGES_QUALITY="ModPagespeedImageRecompressionQuality"
JPEG_QUALITY="ModPagespeedJpegRecompressionQuality"
WEBP_QUALITY="ModPagespeedImageWebpRecompressionQuality"
start_test quality of jpeg output images with generic quality flag
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE=$TEST_ROOT"/image_rewriting/rewrite_images.html"
REWRITE_URL=$IMG_REWRITE"?ModPagespeedFilters=rewrite_images"
URL=$REWRITE_URL"&"$IMAGES_QUALITY"=75"
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
# This filter produces different images on 32 vs 64 bit builds. On 32 bit, the
# size is 8157B, while on 64 it is 8155B. Initial investigation showed no
# visible differences between the generated images.
# TODO(jmaessen) Verify that this behavior is expected.
check_file_size "$OUTDIR/*256x192*Puzzle*" -le 8157   # resized

start_test quality of jpeg output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE=$TEST_ROOT"/jpeg_rewriting/rewrite_images.html"
REWRITE_URL=$IMG_REWRITE"?ModPagespeedFilters=rewrite_images"
URL=$REWRITE_URL",recompress_jpeg&"$IMAGES_QUALITY"=85&"$JPEG_QUALITY"=70"
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
check_file_size "$OUTDIR/*256x192*Puzzle*" -le 7564   # resized

start_test quality of webp output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE=$TEST_ROOT"/webp_rewriting/rewrite_images.html"
REWRITE_URL=$IMG_REWRITE"?ModPagespeedFilters=rewrite_images"
URL=$REWRITE_URL",convert_jpeg_to_webp&"$IMAGES_QUALITY"=75&"$WEBP_QUALITY"=65"
check run_wget_with_args --header 'X-PSA-Blocking-Rewrite: psatest' $URL
check_file_size "$OUTDIR/*webp*" -le 1784   # resized, optimized to webp

# Depends upon "Header append Vary User-Agent" and ModPagespeedRespectVary.
start_test respect vary user-agent
URL=$TEST_ROOT/vary/index.html?ModPagespeedFilters=inline_css
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" fgrep "<style>"

start_test ModPagespeedShardDomain directive in .htaccess file
rm -rf $OUTDIR
mkdir $OUTDIR
echo $WGET_DUMP $TEST_ROOT/shard/shard.html
fetch_until $TEST_ROOT/shard/shard.html 'grep -c \.pagespeed\.' 4
check $WGET_DUMP $TEST_ROOT/shard/shard.html > $OUTDIR/shard.out.html
check [ $(grep -ce href=\"http://shard1 $OUTDIR/shard.out.html) = 2 ];
check [ $(grep -ce href=\"http://shard2 $OUTDIR/shard.out.html) = 2 ];

start_test server-side includes
rm -rf $OUTDIR
mkdir $OUTDIR
echo $WGET_DUMP $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css
fetch_until $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css \
    'grep -c \.pagespeed\.' 1
check $WGET_DUMP $TEST_ROOT/ssi/ssi.shtml?ModPagespeedFilters=combine_css \
  > $OUTDIR/ssi.out.html
check [ $(grep -ce $combine_css_filename $OUTDIR/ssi.out.html) = 1 ];

start_test mod_rewrite
echo $WGET_DUMP $TEST_ROOT/redirect/php/
check $WGET_DUMP $TEST_ROOT/redirect/php/ > $OUTDIR/redirect_php.html
check \
  [ $(grep -ce "href=\"/mod_pagespeed_test/" $OUTDIR/redirect_php.html) = 2 ];

start_test ModPagespeedLoadFromFile
URL=$TEST_ROOT/load_from_file/index.html?ModPagespeedFilters=inline_css
fetch_until $URL 'grep -c blue' 1

# The "httponly" directory is disallowed.
fetch_until $URL 'fgrep -c web.httponly.example.css' 1

# Loading .ssp.css files from file is disallowed.
fetch_until $URL 'fgrep -c web.example.ssp.css' 1

# There's an exception "allow" rule for "exception.ssp.css" so it can be loaded
# directly from the filesystem.
fetch_until $URL 'fgrep -c file.exception.ssp.css' 1

start_test ModPagespeedLoadFromFileMatch
URL=$TEST_ROOT/load_from_file_match/index.html?ModPagespeedFilters=inline_css
fetch_until $URL 'grep -c blue' 1

start_test Custom headers remain on HTML, but cache should be disabled.
URL=$TEST_ROOT/rewrite_compressed_js.html
echo $WGET_DUMP $URL
HTML_HEADERS=$($WGET_DUMP $URL)
check_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1"
# The extra header should only be added once, not twice.
check_not_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1, 1"
check_from "$HTML_HEADERS" egrep -q 'Cache-Control: max-age=0, no-cache'

start_test Custom headers remain on resources, but cache should be 1 year.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
echo $WGET_DUMP $URL
RESOURCE_HEADERS=$($WGET_DUMP $URL)
check_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1'
# The extra header should only be added once, not twice.
check_not_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1, 1'
check_from "$RESOURCE_HEADERS"  egrep -q 'Cache-Control: max-age=31536000'

start_test ModPagespeedModifyCachingHeaders
URL=$TEST_ROOT/retain_cache_control/index.html
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q "Cache-Control: private, max-age=3000"

test_filter combine_javascript combines 2 JS files into 1.
start_test combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?ModPagespeedFilters=combine_javascript
fetch_until $URL 'grep -c src=' 4

start_test aris disables js combining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 2

start_test aris disables js combining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 1

test_filter inline_javascript inlines a small JS file
start_test aris disables js inlining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 1

start_test aris disables js inlining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 0

test_filter rewrite_javascript minifies JavaScript and saves bytes.
start_test aris disables js cache extention for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=rewrite_javascript"
# first check something that should get rewritten to know we're done with
# rewriting
fetch_until $URL 'grep -c "src=\"../normal.js\""' 0
check [ $($WGET_DUMP $URL | grep -c "src=\"../introspection.js\"") = 1 ]

start_test aris disables js cache extension only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=rewrite_javascript"
fetch_until $URL 'grep -c src=\"normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"introspection.js\") = 0 ]

# Check that no filter changes urls for introspective javascript if
# avoid_renaming_introspective_javascript is on
start_test aris disables url modification for introspective js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/?\
ModPagespeedFilters=testing,core"
# first check something that should get rewritten to know we're done with
# rewriting
fetch_until $URL 'grep -c src=\"../normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"../introspection.js\") = 1 ]

start_test aris disables url modification only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html?\
ModPagespeedFilters=testing,core"
fetch_until $URL 'grep -c src=\"normal.js\"' 0
check [ $($WGET_DUMP $URL | grep -c src=\"introspection.js\") = 0 ]

start_test HTML add_instrumentation lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?ModPagespeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]

start_test XHTML add_instrumentation also lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.xhtml\
?ModPagespeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]

start_test flush_subresources rewriter is not applied
URL="$TEST_ROOT/flush_subresources.html?\
ModPagespeedFilters=flush_subresources,extend_cache_css,\
extend_cache_scripts"
# Fetch once with X-PSA-Blocking-Rewrite so that the resources get rewritten and
# property cache is updated with them.
wget -O - --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $TEMPDIR/flush.$$
# Fetch again. The property cache has the subresources this time but
# flush_subresources rewriter is not applied. This is a negative test case
# because this rewriter does not exist in mod_pagespeed yet.
check [ `wget -O - $URL | grep -o 'link rel="subresource"' | wc -l` = 0 ]
rm -f $TEMPDIR/flush.$$

# Note: This tests will fail with default
# WGET_ARGS="--header=ModPagespeedFilters:rewrite_javascript"
WGET_ARGS=""
start_test Respect custom options on resources.
IMG_NON_CUSTOM="$EXAMPLE_ROOT/images/xPuzzle.jpg.pagespeed.ic.fakehash.jpg"
IMG_CUSTOM="$TEST_ROOT/custom_options/xPuzzle.jpg.pagespeed.ic.fakehash.jpg"

# Identical images, but in the .htaccess file in the custom_options directory we
# additionally disable core-filter convert_jpeg_to_progressive which gives a
# larger file.
fetch_until $IMG_NON_CUSTOM 'wc -c' 216942
fetch_until $IMG_CUSTOM 'wc -c' 231192

# Test our handling of headers when a FLUSH event occurs.
# Skip if PHP is not installed to cater for admins who don't want it installed.
# Always fetch the first file so we can check if PHP is enabled.
start_test Headers are not destroyed by a flush event.
FILE=php_withoutflush.php
URL=$TEST_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
$WGET_DUMP $URL > $FETCHED
if grep -q '<?php' $FETCHED; then
  echo "*** Skipped because PHP is not installed. If you'd like to enable this"
  echo "*** test please run: sudo apt-get install php5-common php5"
else
  check [ $(grep -c '^X-Mod-Pagespeed:'               $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: without_flush' $FETCHED) = 1 ]
  check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]

  FILE=php_withflush.php
  URL=$TEST_ROOT/$FILE
  FETCHED=$OUTDIR/$FILE
  $WGET_DUMP $URL > $FETCHED
  check [ $(grep -c '^X-Mod-Pagespeed:'               $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: with_flush'    $FETCHED) = 1 ]
  # 2.2 prefork returns no content length while 2.2 worker returns a real
  # content length. IDK why but skip this test because of that.
  # check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]
fi

start_test ModPagespeedMapProxyDomain
URL=$EXAMPLE_ROOT/proxy_external_resource.html
echo Rewrite HTML with reference to a proxyable image.
fetch_until -save -recursive $URL \
    'grep -c pss_images/xPuzzle\.jpg\.pagespeed\.ic' 1
check_file_size "$OUTDIR/xPuzzle*" -lt 60000

# To make sure that we can reconstruct the proxied content by going back
# to the origin, we must avoid hitting the output cache.
# Note that cache-flushing does not affect the cache of rewritten resources;
# only input-resources and metadata.  To avoid hitting that cache and force
# us to rewrite the resource from origin, we grab this resource from a
# virtual host attached to a different cache.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  # With the proper hash, we'll get a long cache lifetime.
  SECONDARY_HOST="http://secondary.example.com/pss_images"
  PROXIED_IMAGE="$SECONDARY_HOST/$(basename $OUTDIR/xPuzzle*)"
  WGET_ARGS="--save-headers"

  echo $PROXIED_IMAGE expecting one year cache.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $PROXIED_IMAGE \
      "grep -c max-age=31536000" 1

  # With the wrong hash, we'll get a short cache lifetime (and also no output
  # cache hit.
  WRONG_HASH="0"
  PROXIED_IMAGE="$SECONDARY_HOST/xPuzzle.jpg.pagespeed.ic.$WRONG_HASH.jpg"
  echo Fetching $PROXIED_IMAGE expecting short private cache.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $PROXIED_IMAGE \
      "grep -c max-age=300,private" 1

  WGET_ARGS=""
fi

# TODO(sligocki): start_test ModPagespeedMaxSegmentLength

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  WGET_ARGS=""

  start_test add_instrumentation has added unload handler with \
      ModPagespeedReportUnloadTime enabled in APACHE_SECONDARY_PORT.
  URL="$SECONDARY_TEST_ROOT/add_instrumentation.html\
?ModPagespeedFilters=add_instrumentation"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
  http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
  check [ $(grep -c "<script" $WGET_OUTPUT) = 3 ]
  check [ $(grep -c "pagespeed.addInstrumentationInit('/mod_pagespeed_beacon?ets=', 'beforeunload', '', '', '', 'http://secondary.example.com/mod_pagespeed_test/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
  check [ $(grep -c "pagespeed.addInstrumentationInit('/mod_pagespeed_beacon?ets=', 'load', '', '', '', 'http://secondary.example.com/mod_pagespeed_test/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test When ModPagespeedMaxHtmlParseBytes is not set, we do not insert \
        a redirect.
    $WGET -O $WGET_OUTPUT \
        $SECONDARY_TEST_ROOT/large_file.html?ModPagespeedFilters=
    check [ $(grep -c "window.location=" $WGET_OUTPUT) = 0 ]
  fi

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
  echo $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  $SUDO touch $MOD_PAGESPEED_CACHE/cache.flush
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush

  URL_PATH=cache_flush_test.html?ModPagespeedFilters=inline_css
  URL=$TEST_ROOT/$URL_PATH
  CSS_FILE=$APACHE_DOC_ROOT/mod_pagespeed_test/update.css
  TMP_CSS_FILE=$TEMPDIR/update.css.$$

  # First, write color 0 into the css file and make sure it gets inlined into
  # the html.
  echo "echo \".class myclass { color: $COLOR0; }\" > $CSS_FILE"
  echo ".class myclass { color: $COLOR0; }" >$TMP_CSS_FILE
  chmod ugo+r $TMP_CSS_FILE  # in case the user's umask doesn't allow o+r
  $SUDO cp $TMP_CSS_FILE $CSS_FILE
  fetch_until $URL "grep -c $COLOR0" 1

  # Also do the same experiment using a different VirtualHost.  It points
  # to the same htdocs, but uses a separate cache directory.
  SECONDARY_URL=$SECONDARY_TEST_ROOT/$URL_PATH
  http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR0" 1

  # Track how many flushes were noticed by Apache processes up till
  # this point in time.  Note that each Apache process/vhost
  # separately detects the 'flush'.
  NUM_INITIAL_FLUSHES=$(scrape_stat cache_flush_count)

  # Now change the file to $COLOR1.
  echo echo ".class myclass { color: $COLOR1; }" ">" $CSS_FILE
  echo ".class myclass { color: $COLOR1; }" >$TMP_CSS_FILE
  $SUDO cp $TMP_CSS_FILE $CSS_FILE

  # We might have stale cache for 5 minutes, so the result might stay
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
  fetch_until $URL "grep -c $COLOR1" 1

  # TODO(jmarantz): we can change this test to be more exacting now, since
  # to address Issue 568, we should only get one cache-flush bump every time
  # we touch the file.
  NUM_FLUSHES=$(scrape_stat cache_flush_count)
  NUM_NEW_FLUSHES=$(expr $NUM_FLUSHES - $NUM_INITIAL_FLUSHES)
  echo NUM_NEW_FLUSHES = $NUM_FLUSHES - $NUM_INITIAL_FLUSHES = $NUM_NEW_FLUSHES
  check [ $NUM_NEW_FLUSHES -ge 1 ]
  check [ $NUM_NEW_FLUSHES -lt 20 ]

  # However, the secondary cache might not have seen this cache-flush, but
  # due to the multiple child processes, each of which does polling separately,
  # we cannot guarantee it.  I think if we knew we were running a 'worker' mpm
  # with just 1 child process we could do this test.
  # fetch_until $SECONDARY_URL 'grep -c blue' 1

  # Now flush the secondary cache too so it can see the change to $COLOR1.
  echo $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  $SUDO touch ${MOD_PAGESPEED_CACHE}/_secondary/cache.flush
  http_proxy=$SECONDARY_HOSTNAME fetch_until $SECONDARY_URL "grep -c $COLOR1" 1

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
  # Because of the empty cache requirement, we conditionalize it on a single
  # value of NO_VHOST_MERGE, so it runs only once per apache_debug_smoke_test
  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test Connection refused handling

    # Monitor the Apache log starting now.  tail -F will catch log rotations.
    SERF_REFUSED_PATH=$TEMPDIR/instaweb_apache_serf_refused.$$
    rm $SERF_REFUSED_PATH
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
      ERRS=$(grep -c "Serf status 111" $SERF_REFUSED_PATH)
      if [ $ERRS -ge 1 ]; then
        break;
      fi;
      echo -n "."
      sleep 0.1
    done;
    echo "."
    # Kill the log monitor silently.
    kill $TAIL_PID
    wait $TAIL_PID 2> /dev/null
    check [ $ERRS -ge 1 ]
    # Make sure we have the URL detail we expect because
    # ModPagespeedListOutstandingUrlsOnError is on in debug.conf.template.
    echo Check that ModPagespeedSerfListOutstandingUrlsOnError works
    check grep "URL http://modpagespeed.com:1023/someimage.png active for " \
        $SERF_REFUSED_PATH

    # Likewise, blocking rewrite tests are only run once.
    start_test Blocking rewrite enabled.
    # We assume that blocking_rewrite_test_dont_reuse_1.jpg will not be
    # rewritten on the first request since it takes significantly more time to
    # rewrite than the rewrite deadline and it is not already accessed by
    # another request earlier.
    BLOCKING_REWRITE_URL="$TEST_ROOT/blocking_rewrite.html?\
ModPagespeedFilters=rewrite_images"
    OUTFILE=$OUTDIR/blocking_rewrite.out.html
    OLDSTATS=$OUTDIR/blocking_rewrite_stats.old
    NEWSTATS=$OUTDIR/blocking_rewrite_stats.new
    $WGET_DUMP $STATISTICS_URL > $OLDSTATS
    check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest'\
      $BLOCKING_REWRITE_URL > $OUTFILE
    $WGET_DUMP $STATISTICS_URL > $NEWSTATS
    check_stat $OLDSTATS $NEWSTATS image_rewrites 1
    check_stat $OLDSTATS $NEWSTATS cache_hits 0
    check_stat $OLDSTATS $NEWSTATS cache_misses 1
    check_stat $OLDSTATS $NEWSTATS cache_inserts 2
    check_stat $OLDSTATS $NEWSTATS num_rewrites_executed 1

    start_test Blocking rewrite enabled using wrong key.
    BLOCKING_REWRITE_URL="$SECONDARY_TEST_ROOT/\
blocking_rewrite_another.html?ModPagespeedFilters=rewrite_images"
    OUTFILE=$OUTDIR/blocking_rewrite.out.html
    http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: junk' \
      $BLOCKING_REWRITE_URL > $OUTFILE
    check [ $(grep -c "[.]pagespeed[.]" $OUTFILE) -lt 1 ]

    http_proxy=$SECONDARY_HOSTNAME fetch_until $BLOCKING_REWRITE_URL 'grep -c [.]pagespeed[.]' 1
  fi

  # http://code.google.com/p/modpagespeed/issues/detail?id=494 -- test
  # that fetching a css with embedded relative images from a different
  # VirtualHost, accessing the same content, and rewrite-mapped to the
  # primary domain, delivers results that are cached for a year, which
  # implies the hash matches when serving vs when rewriting from HTML.
  #
  # This rewrites the CSS, absolutifying the embedded relative image URL
  # reference based on the the main server host.
  start_test Relative images embedded in a CSS file served from a mapped domain
  WGET_ARGS=""
  DIR="mod_pagespeed_test/map_css_embedded"
  URL="http://www.example.com/$DIR/issue494.html"
  MAPPED_CSS="$DIR/I.styles.css.pagespeed.cf.OOyfQ_LoNP.css"
  http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
      "grep -c cdn.example.com/$MAPPED_CSS" 1

  # Now fetch the resource using a different host, which is mapped to the first
  # one.  To get the correct bytes, matching hash, and long TTL, we need to do
  # apply the domain mapping in the CSS resource fetch.
  CSS_OUT="$OUTDIR/mapped_css.$$"
  URL="http://origin.example.com/$MAPPED_CSS"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL '>' $CSS_OUT
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL > $CSS_OUT
  check fgrep -q "Cache-Control: max-age=31536000" $CSS_OUT
  rm -f $CSS_OUT

  # Test ModPagespeedForbidFilters, which is set in pagespeed.conf for the VHost
  # forbidden.example.com, where we've forbidden remove_quotes, remove_comments,
  # collapse_whitespace, rewrite_css, and resize_images; we've also disabled
  # inline_css so the link doesn't get inlined since we test that it still has
  # all its quotes.
  FORBIDDEN_TEST_ROOT=http://forbidden.example.com/mod_pagespeed_test
  function test_forbid_filters() {
    QUERYP="$1"
    HEADER="$2"
    URL="$FORBIDDEN_TEST_ROOT/forbidden.html"
    OUTFILE="$TEMPDIR/test_forbid_filters.$$"
    echo http_proxy=$SECONDARY_HOSTNAME $WGET $HEADER $URL$QUERYP
    http_proxy=$SECONDARY_HOSTNAME $WGET -q -O $OUTFILE $HEADER $URL$QUERYP
    check egrep -q '<link rel="stylesheet' $OUTFILE
    check egrep -q '<!--'                  $OUTFILE
    check egrep -q '    <li>'              $OUTFILE
    rm -f $OUTFILE
  }
  start_test ModPagespeedForbidFilters baseline check.
  test_forbid_filters "" ""
  start_test ModPagespeedForbidFilters query parameters check.
  QUERYP="?ModPagespeedFilters="
  QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace
  test_forbid_filters $QUERYP ""
  start_test ModPagespeedForbidFilters request headers check."
  HEADER="--header=ModPagespeedFilters:"
  HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
  test_forbid_filters "" $HEADER

  start_test ModPagespeedForbidFilters disallows direct resource rewriting.
  FORBIDDEN_EXAMPLE_ROOT=http://forbidden.example.com/mod_pagespeed_example
  FORBIDDEN_STYLES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/styles
  FORBIDDEN_IMAGES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/images
  # .ce. is allowed
  ALLOWED="$FORBIDDEN_STYLES_ROOT/all_styles.css.pagespeed.ce.n7OstQtwiS.css"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O /dev/null $ALLOWED 2>&1)
  check_from "$OUT" fgrep -q "200 OK"
  # .cf. is forbidden
  FORBIDDEN=$FORBIDDEN_STYLES_ROOT/I.all_styles.css.pagespeed.cf.UH8L-zY4b4.css
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O /dev/null $FORBIDDEN 2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"
  # The image will be optimized but NOT resized to the much smaller size,
  # so it will be >200k (optimized) rather than <20k (resized).
  # Use a blocking fetch to force all -allowed- rewriting to be done.
  RESIZED=$FORBIDDEN_IMAGES_ROOT/256x192xPuzzle.jpg.pagespeed.ic.8AB3ykr7Of.jpg
  HEADERS="$OUTDIR/headers.$$"
  http_proxy=$SECONDARY_HOSTNAME $WGET -q --server-response -O /dev/null \
    --header 'X-PSA-Blocking-Rewrite: psatest' $RESIZED >& $HEADERS
  LENGTH=$(grep '^ *Content-Length:' $HEADERS | sed -e 's/.*://')
  check test -n "$LENGTH"
  check test $LENGTH -gt 200000
  CCONTROL=$(grep '^ *Cache-Control:' $HEADERS | sed -e 's/.*://')
  check_from "$CCONTROL" grep -w max-age=300
  check_from "$CCONTROL" grep -w private
fi

WGET_ARGS=""
start_test Send custom fetch headers on resource re-fetches.
PLAIN_HEADER="header=value"
X_OTHER_HEADER="x-other=False"

URL="http://$HOSTNAME/mod_pagespeed_log_request_headers.js.pagespeed.jm.0.js"
WGET_OUT=$($WGET_DUMP $URL)
check_from "$WGET_OUT" grep "$PLAIN_HEADER"
check_from "$WGET_OUT" grep "$X_OTHER_HEADER"

start_test Send custom fetch headers on resource subfetches.
URL=$TEST_ROOT/custom_fetch_headers.html?ModPagespeedFilters=inline_javascript
fetch_until -save $URL 'grep -c header=value' 1
check_from "$(cat $FETCH_FILE)" grep "$X_OTHER_HEADER"

# Check that statistics logging was functional during these tests
# if it was enabled.
if egrep -q "^    # ModPagespeedStatistics off$" $APACHE_DEBUG_PAGESPEED_CONF &&
   egrep -q "^ ModPagespeedStatisticsLogging on$" $APACHE_DEBUG_PAGESPEED_CONF;
   then
  start_test Statistics logging works.
  check [ $(grep "timestamp: " $MOD_PAGESPEED_STATS_LOG* | wc -l) -ge 1 ]
  # An array of all the timestamps that were logged.
  TIMESTAMPS=($(sed -n '/timestamp: /s/[^0-9]*//gp' $MOD_PAGESPEED_STATS_LOG*))
  check [ ${#TIMESTAMPS[@]} -ge 1 ]
  for T in ${TIMESTAMPS[@]}; do
    check [ $T -ge $START_TIME ]
  done
  # Check a few arbitrary statistics to make sure logging is taking place.
  check [ $(grep "num_flushes: " $MOD_PAGESPEED_STATS_LOG* | wc -l) -ge 1 ]
  check [ $(grep "histogram#" $MOD_PAGESPEED_STATS_LOG* | wc -l) -ge 1 ]
  check [ $(grep "image_ongoing_rewrites: " $MOD_PAGESPEED_STATS_LOG* | wc -l) \
      -ge 1 ]

  start_test Statistics logging JSON handler works.
  JSON=$OUTDIR/console_json.json
  STATS_JSON_URL="$(echo $STATISTICS_URL)?json&granularity=0&var_titles=num_\
flushes,image_ongoing_rewrites&hist_titles=Html%20Time%20us%20Histogram"
  $WGET_DUMP $STATS_JSON_URL > $JSON
  # Each variable we ask for should show up once.
  check [ $(grep "\"num_flushes\": " $JSON | wc -l) -eq 1 ]
  check [ $(grep "\"image_ongoing_rewrites\": " $JSON | wc -l) -eq 1 ]
  check [ $(grep "\"Html Time us Histogram\": " $JSON | wc -l) -eq 1 ]
  check [ $(grep "\"timestamps\": " $JSON | wc -l) -eq 1 ]
  # An array of all the timestamps that the JSON handler returned.
  JSON_TIMESTAMPS=($(sed -rn 's/^\{"timestamps": \[(([0-9]+, )*[0-9]*)\].*}$/\1/;/^[0-9]+/s/,//gp' $JSON))
  # Check that we see the same timestamps that are in TIMESTAMPS.
  # We might have generated extra timestamps in the time between TIMESTAMPS
  # and JSON_TIMESTAMPS, so only loop through TIMESTAMPS.
  check [ ${#JSON_TIMESTAMPS[@]} -ge ${#TIMESTAMPS[@]} ]
  t=0
  while [ $t -lt ${#TIMESTAMPS[@]} ]; do
    check [ ${TIMESTAMPS[$t]} -eq ${JSON_TIMESTAMPS[$t]} ]
    t=$(($t+1))
  done

  start_test Statistics console is available.
  CONSOLE_URL=http://$HOSTNAME/mod_pagespeed_console
  CONSOLE_HTML=$OUTDIR/console.html
  $WGET_DUMP $CONSOLE_URL > $CONSOLE_HTML
  check grep -q "console" $CONSOLE_HTML
fi

start_test ModPagespeedIf parsing
readonly CONFIG_URL=$STATISTICS_URL?config
readonly SPDY_CONFIG_URL=$STATISTICS_URL?spdy_config

echo $WGET_DUMP $CONFIG_URL
CONFIG=$($WGET_DUMP $CONFIG_URL)
check_from "$CONFIG" egrep -q "Configuration:"
check_not_from "$CONFIG" egrep -q "SPDY-specific configuration:"
# Regular config should have a shard line:
check_from "$CONFIG" egrep -q "http://nonspdy.example.com/ Auth Shards:{http:"
check_from "$CONFIG" egrep -q "//s1.example.com/, http://s2.example.com/}"
# And "combine CSS" on.
check_from "$CONFIG" egrep -q "Combine Css"

echo $WGET_DUMP $SPDY_CONFIG_URL
SPDY_CONFIG=$($WGET_DUMP $SPDY_CONFIG_URL)
check_not_from "$SPDY_CONFIG" egrep -q "Configuration:"
check_from "$SPDY_CONFIG" egrep -q "SPDY-specific configuration:"

# SPDY config should have neither shards, nor combine CSS.
check_not_from "$SPDY_CONFIG" egrep -q "http://nonspdy.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "s1.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "s2.example.com"
check_not_from "$SPDY_CONFIG" egrep -q "Combine Css"

# Test ModPagespeedForbidAllDisabledFilters, which is set in pagespeed.conf
# for /mod_pagespeed_test/forbid_all_disabled/disabled/ where we've disabled
# remove_quotes, remove_comments, and collapse_whitespace (which are enabled
# by its parent directory's .htaccess file). We fetch 3 x 3 times, the first 3
# being for forbid_all_disabled, fordid_all_disabled/disabled, and
# forbid_all_disabled/disabled/cheat, to ensure that a subdirectory cannot
# circumvent the forbidden flag; and the second 3 being a normal fetch, a
# fetch using a query parameter to try to enable the forbidden filters, and a
# fetch using a request header to try to enable the forbidden filters.
function test_forbid_all_disabled() {
  QUERYP="$1"
  HEADER="$2"
  if [ -n "$QUERYP" ]; then
    INLINE_CSS=",-inline_css"
  else
    INLINE_CSS="?ModPagespeedFilters=-inline_css"
  fi
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  URL1=$TEST_ROOT/forbid_all_disabled/forbidden.html
  URL2=$TEST_ROOT/forbid_all_disabled/disabled/forbidden.html
  URL3=$TEST_ROOT/forbid_all_disabled/disabled/cheat/forbidden.html
  OUTFILE="$TEMPDIR/test_forbid_all_disabled.$$"
  # Fetch testing that forbidden filters stay disabled.
  echo $WGET $HEADER $URL1$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1$QUERYP$INLINE_CSS
  check     egrep -q '<link rel=stylesheet'  $OUTFILE
  check_not egrep -q '<!--'                  $OUTFILE
  check     egrep -q '^<li>'                 $OUTFILE
  echo $WGET $HEADER $URL2$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  echo $WGET $HEADER $URL3$QUERYP$INLINE_CSS
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3$QUERYP$INLINE_CSS
  check     egrep -q '<link rel="stylesheet' $OUTFILE
  check     egrep -q '<!--'                  $OUTFILE
  check     egrep -q '    <li>'              $OUTFILE
  # Fetch testing that enabling inline_css in disabled/.htaccess works.
  echo $WGET $HEADER $URL1
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL1
  check_not egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL2
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL2
  check     egrep -q '<style>.yellow'        $OUTFILE
  echo $WGET $HEADER $URL3
  $WGET $WGET_ARGS -q -O $OUTFILE $HEADER $URL3
  check     egrep -q '<style>.yellow'        $OUTFILE
  rm -f $OUTFILE
  unset WGET_ARGS
}
start_test ModPagespeedForbidAllDisabledFilters baseline check.
test_forbid_all_disabled "" ""
start_test ModPagespeedForbidAllDisabledFilters query parameters check.
QUERYP="?ModPagespeedFilters="
QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled $QUERYP ""
start_test ModPagespeedForbidAllDisabledFilters request headers check.
HEADER="--header=ModPagespeedFilters:"
HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
test_forbid_all_disabled "" $HEADER

# Now check stuff on secondary host. The results will depend on whether
# ModPagespeedInheritVHostConfig is on or off. We run this only for some tests,
# since we don't always have the secondary port number available here.
if [ x$SECONDARY_HOSTNAME != x ]; then
  SECONDARY_STATS_URL=http://$SECONDARY_HOSTNAME/mod_pagespeed_statistics
  SECONDARY_CONFIG_URL=$SECONDARY_STATS_URL?config
  SECONDARY_SPDY_CONFIG_URL=$SECONDARY_STATS_URL?spdy_config

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    start_test Config with VHost inheritance off
    echo $WGET_DUMP $SECONDARY_CONFIG_URL
    SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
    check_from "$SECONDARY_CONFIG" egrep -q "Configuration:"
    check_not_from "$SECONDARY_CONFIG" egrep -q "SPDY-specific configuration:"
    # No inherit, no sharding.
    check_not_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

    # Should not inherit the blocking rewrite key.
    check_not_from "$SECONDARY_CONFIG" egrep -q "blrw"

    echo $WGET_DUMP $SECONDARY_SPDY_CONFIG_URL
    SECONDARY_SPDY_CONFIG=$($WGET_DUMP $SECONDARY_SPDY_CONFIG_URL)
    check_not_from "$SECONDARY_SPDY_CONFIG" egrep -q "Configuration:"
    check_from "$SECONDARY_SPDY_CONFIG" \
      egrep -q "SPDY-specific configuration missing"
  else
    start_test Config with VHost inheritance on
    echo $WGET_DUMP $SECONDARY_CONFIG_URL
    SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
    check_from "$SECONDARY_CONFIG" egrep -q "Configuration:"
    check_not_from "$SECONDARY_CONFIG" egrep -q "SPDY-specific configuration:"
    # Sharding is applied in this host, thanks to global inherit flag.
    check_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

    # We should also inherit the blocking rewrite key.
    check_from "$SECONDARY_CONFIG" egrep -q "blrw[[:space:]]+psatest"

    echo $WGET_DUMP $SECONDARY_SPDY_CONFIG_URL
    SECONDARY_SPDY_CONFIG=$($WGET_DUMP $SECONDARY_SPDY_CONFIG_URL)
    check_not_from "$SECONDARY_SPDY_CONFIG" egrep -q "Configuration:"
    check_from "$SECONDARY_SPDY_CONFIG" egrep -q "SPDY-specific configuration:"
    # Disabling of combine CSS should get inherited.
    check_not_from "$SECONDARY_SPDY_CONFIG" egrep -q "Combine Css"
  fi
fi

# Test max_cacheable_response_content_length.  There are two Javascript files
# in the html file.  The smaller Javascript file should be rewritten while
# the larger one shouldn't.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test Maximum length of cacheable response content.
  HOST_NAME="http://max_cacheable_content_length.example.com"
  DIR_NAME="mod_pagespeed_test/max_cacheable_content_length"
  HTML_NAME="test_max_cacheable_content_length.html"
  URL=$HOST_NAME/$DIR_NAME/$HTML_NAME
  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header \
      'X-PSA-Blocking-Rewrite: psatest' $URL)
  check_from     "$RESPONSE_OUT" fgrep -qi small.js.pagespeed.
  check_not_from "$RESPONSE_OUT" fgrep -qi large.js.pagespeed.
fi

# Test handling of large HTML files. We first test with a cold cache, and verify
# that we bail out of parsing and insert a script redirecting to
# ?ModPagespeed=off. This should also insert an entry into the property cache so
# that the next time we fetch the file it will not be parsed at all.
echo TEST: Handling of large files.
# Add a timestamp to the URL to ensure it's not in the property cache.
FILE="max_html_parse_size/large_file.html?value=$(date +%s)"
URL=$TEST_ROOT/$FILE
# Enable a filter that will modify something on this page, since we testing that
# this page should not be rewritten.
WGET_ARGS="--header=ModPagespeedFilters:rewrite_images"
WGET_EC="$WGET_DUMP $WGET_ARGS"
echo $WGET_EC $URL
LARGE_OUT=$($WGET_EC $URL)
check_from "$LARGE_OUT" grep -q window.location=".*&ModPagespeed=off"

# The file should now be in the property cache so make sure that the page is no
# longer parsed. Use fetch_until because we need to wait for a potentially
# non-blocking write to the property cache from the previous test to finish
# before this will succeed.
fetch_until -save $URL 'grep -c window.location=".*&ModPagespeed=off"' 0
check_not fgrep -q pagespeed.ic $FETCH_UNTIL_OUTFILE

# TODO(matterbury): Uncomment these lines then the test is fixed.
:<< COMMENTING_BLOCK
start_test ModPagespeedIf application
# Without SPDY, we should combine things
OUT=$($WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' \
    $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q ',Mcc'

# Despite combine_css being disabled in <ModPagespeedIf>, we still
# expect it with SPDY since it's turned on in mod_pagespeed_example/.htaccess.
# However, since rewrite_css is off, the result should be rewritten by
# cc and not also cf or ce.
OUT=$($WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' \
  --header 'X-PSA-Optimize-For-SPDY: true' \
  $EXAMPLE_ROOT/combine_css.html)
check_not_from "$OUT" egrep -q ',Mcc'
check_from "$OUT" egrep -q '.pagespeed.cc'

# Now test resource fetch. Since we've disabled extend_cache and
# rewrite_images for spdy, we should not see rewritten resources there,
# while we will in the other normal case.
OUT=$($WGET_DUMP  --header 'X-PSA-Blocking-Rewrite: psatest' \
    $EXAMPLE_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.rnLTdExmOm.css)
check_from "$OUT" grep -q 'png.pagespeed.'

OUT=$($WGET_DUMP  --header 'X-PSA-Blocking-Rewrite: psatest' \
    --header 'X-PSA-Optimize-For-SPDY: true' \
    $EXAMPLE_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.rnLTdExmOm.css)
check_not_from "$OUT" grep -q 'png.pagespeed.'
COMMENTING_BLOCK
# Cleanup
rm -rf $OUTDIR

system_test_trailer
