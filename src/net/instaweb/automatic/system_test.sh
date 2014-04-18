#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Generic system test, which should work on any implementation of Page Speed
# Automatic.
#
# See system_test_helpers.sh for usage.
#
# The shell script sourcing this one is expected to be implementation specific
# and have its own additional system tests that it runs.  After it finishes,
# the sourcing script should call check_failures_and_exit.  That will print the
# names of any failing tests and exit with status 1 if there are any.
#

# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/system_test_helpers.sh" || exit 1

# General system tests

start_test Page Speed Automatic is running and writes the expected header.
echo $WGET_DUMP $EXAMPLE_ROOT/combine_css.html

HTTP_FILE=$OUTDIR/http_file
# Note: We pipe this to a file instead of storing it in a variable because
# saving strings to variables converts "\n" -> " " inexplicably.
$WGET_DUMP $EXAMPLE_ROOT/combine_css.html > $HTTP_FILE

echo Checking for X-Mod-Pagespeed header
check egrep -q 'X-Mod-Pagespeed|X-Page-Speed' $HTTP_FILE

echo "Checking that we don't have duplicate X-Mod-Pagespeed headers"
check [ $(egrep -c 'X-Mod-Pagespeed|X-Page-Speed' $HTTP_FILE) = 1 ]

echo "Checking that we don't have duplicate headers"
# Note: uniq -d prints only repeated lines. So this should only != "" if
# There are repeated lines in header.
repeat_lines=$(grep ":" $HTTP_FILE | sort | uniq -d)
check [ "$repeat_lines" = "" ]

echo Checking for lack of E-tag
check_not fgrep -i Etag $HTTP_FILE

echo Checking for presence of Vary.
check fgrep -qi 'Vary: Accept-Encoding' $HTTP_FILE

echo Checking for absence of Last-Modified
check_not fgrep -i 'Last-Modified' $HTTP_FILE

# Note: This is in flux, we can now allow cacheable HTML and this test will
# need to be updated if this is turned on by default.
echo Checking for presence of Cache-Control: max-age=0, no-cache
check fgrep -qi 'Cache-Control: max-age=0, no-cache' $HTTP_FILE

echo Checking for absence of X-Frame-Options: SAMEORIGIN
check_not fgrep -i "X-Frame-Options" $HTTP_FILE

# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.
start_test directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q $EXAMPLE_ROOT/?PageSpeed=off -O $OUTDIR/mod_pagespeed_example
check $WGET -q $EXAMPLE_ROOT/index.html?PageSpeed=off -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example

start_test compression is enabled for HTML.
OUT=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' $EXAMPLE_ROOT/ 2>&1)
check_from "$OUT" fgrep -qi 'Content-Encoding: gzip'

start_test X-Mod-Pagespeed header added when PageSpeed=on
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html?PageSpeed=on)
check_from "$OUT" egrep -q 'X-Mod-Pagespeed|X-Page-Speed'

start_test X-Mod-Pagespeed header not added when PageSpeed=off
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html?PageSpeed=off)
check_not_from "$OUT" egrep 'X-Mod-Pagespeed|X-Page-Speed'

start_test We behave sanely on whitespace served as HTML
OUT=$($WGET_DUMP $TEST_ROOT/whitespace.html)
check_from "$OUT" egrep -q 'HTTP/1[.]. 200 OK'

start_test Query params and headers are recognized in resource flow.
URL=$REWRITTEN_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.Hash.css
echo "Image gets rewritten by default."
# TODO(sligocki): Replace this fetch_until with single blocking fetch once
# the blocking rewrite header below works correctly.
WGET_ARGS="--header='X-PSA-Blocking-Rewrite:psatest'"
fetch_until $URL 'fgrep -c BikeCrashIcn.png.pagespeed.ic' 1
echo "Image doesn't get rewritten when we turn it off with headers."
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  --header="PageSpeedFilters:-convert_png_to_jpeg,-recompress_png" $URL)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"

# TODO(vchudnov): This test is not doing quite what it advertises. It
# seems to be getting the cached rewritten resource from the previous
# test case and not going into image.cc itself. Removing the previous
# test case causes this one to go into image.cc. We should test with a
# different resource.
echo "Image doesn't get rewritten when we turn it off with query params."
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  $URL?PageSpeedFilters=-convert_png_to_jpeg,-recompress_png)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"

start_test In-place resource optimization
FETCHED=$OUTDIR/ipro
# Note: we intentionally want to use an image which will not appear on
# any HTML pages, and thus will not be in cache before this test is run.
# (Since the system_test is run multiple times without clearing the cache
# it may be in cache on some of those runs, but we know that it was put in
# the cache by previous runs of this specific test.)
URL=$TEST_ROOT/ipro/test_image_dont_reuse.png
# Size between original image size and rewritten image size (in bytes).
# Used to figure out whether the returned image was rewritten or not.
THRESHOLD_SIZE=13000

# Check that we compress the image (with IPRO).
# Note: This requests $URL until it's size is less than $THRESHOLD_SIZE.
fetch_until -save $URL "wc -c" $THRESHOLD_SIZE "--save-headers" "-lt"
check_file_size $FETCH_FILE -lt $THRESHOLD_SIZE
# Check that resource is served with small Cache-Control header (since
# we cannot cache-extend resources served under the original URL).
# Note: tr -d '\r' is needed because HTTP spec requires lines to end in \r\n,
# but sed does not treat that as $.
echo sed -n 's/Cache-Control: max-age=\([0-9]*\)$/\1/p' $FETCH_FILE
check [ "$(tr -d '\r' < $FETCH_FILE | \
           sed -n 's/Cache-Control: max-age=\([0-9]*\)$/\1/p')" \
        -lt 1000 ]

# Check that the original image is greater than threshold to begin with.
check $WGET_DUMP -O $FETCHED $URL?PageSpeed=off
check_file_size $FETCHED -gt $THRESHOLD_SIZE

# Individual filter tests, in alphabetical order

test_filter add_instrumentation adds 2 script tags
check run_wget_with_args $URL
# Counts occurances of '<script' in $FETCHED
# See: http://superuser.com/questions/339522
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 2 ]

start_test "We don't add_instrumentation if URL params tell us not to"
FILE=add_instrumentation.html?PageSpeedFilters=
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args $URL
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 0 ]

# http://code.google.com/p/modpagespeed/issues/detail?id=170
start_test "Make sure 404s aren't rewritten"
# Note: We run this in the add_instrumentation section because that is the
# easiest to detect which changes every page
THIS_BAD_URL=$BAD_RESOURCE_URL?PageSpeedFilters=add_instrumentation
# We use curl, because wget does not save 404 contents
OUT=$($CURL --silent $THIS_BAD_URL)
check_not_from "$OUT" fgrep "/mod_pagespeed_beacon"

# Checks that we can correctly identify a known library url.
test_filter canonicalize_javascript_libraries finds library urls
fetch_until $URL 'fgrep -c http://www.modpagespeed.com/rewrite_javascript.js' 1

test_filter collapse_whitespace removes whitespace, but not from pre tags.
check run_wget_with_args $URL
check [ $(egrep -c '^ +<' $FETCHED) -eq 1 ]

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'fgrep -c text/css' 1
check run_wget_with_args $URL
#test_resource_ext_corruption $URL $combine_css_filename

start_test combine_css without hash field should 404
echo run_wget_with_args $REWRITTEN_ROOT/styles/yellow.css+blue.css.pagespeed.cc..css
run_wget_with_args $REWRITTEN_ROOT/styles/yellow.css+blue.css.pagespeed.cc..css
check fgrep "404 Not Found" $WGET_OUTPUT

# Note: this large URL can only be processed by Apache if
# ap_hook_map_to_storage is called to bypass the default
# handler that maps URLs to filenames.
start_test Fetch large css_combine URL
LARGE_URL="$REWRITTEN_ROOT/styles/yellow.css+blue.css+big.css+\
bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+bold.css+yellow.css+blue.css+\
big.css+bold.css+yellow.css+blue.css+big.css+\
bold.css.pagespeed.cc.46IlzLf_NK.css"
echo "$WGET --save-headers -q -O - $LARGE_URL | head -1 | egrep \"HTTP/1[.]. 200 OK\""
OUT=$($WGET --save-headers -q -O - $LARGE_URL | head -1)
check_from "$OUT" egrep -q "HTTP/1[.]. 200 OK"
LARGE_URL_LINE_COUNT=$($WGET -q -O - $LARGE_URL | wc -l)
echo Checking that response body is at least 900 lines -- it should be 954
check [ $LARGE_URL_LINE_COUNT -gt 900 ]

test_filter combine_javascript combines 2 JS files into 1.
fetch_until $URL 'fgrep -c src=' 1
check run_wget_with_args $URL

start_test combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?PageSpeedFilters=combine_javascript
fetch_until $URL 'fgrep -c src=' 4

test_filter combine_heads combines 2 heads into 1
check run_wget_with_args $URL
check [ $(fgrep -c '<head>' $FETCHED) = 1 ]

test_filter elide_attributes removes boolean and default attributes.
check run_wget_with_args $URL
check_not fgrep "disabled=" $FETCHED   # boolean, should not find

test_filter extend_cache_images rewrites an image tag.
URL=$EXAMPLE_ROOT/extend_cache.html?PageSpeedFilters=extend_cache_images
fetch_until $URL 'egrep -c src.*/Puzzle[.]jpg[.]pagespeed[.]ce[.].*[.]jpg' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
#test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

start_test Attempt to fetch cache-extended image without hash should 404
run_wget_with_args $REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce..jpg
check fgrep "404 Not Found" $WGET_OUTPUT

start_test Cache-extended image should respond 304 to an If-Modified-Since.
URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
DATE=$(date -R)
run_wget_with_args --header "If-Modified-Since: $DATE" $URL
check fgrep "304 Not Modified" $WGET_OUTPUT

start_test Legacy format URLs should still work.
URL=$REWRITTEN_ROOT/images/ce.0123456789abcdef0123456789abcdef.Puzzle,j.jpg
# Note: Wget request is HTTP/1.0, so some servers respond back with
# HTTP/1.0 and some respond back 1.1.
$WGET_DUMP $URL > $FETCHED
check egrep -q 'HTTP/1[.]. 200 OK' $FETCHED

start_test Filters do not rewrite blacklisted JavaScript files.
URL=$TEST_ROOT/blacklist/blacklist.html?PageSpeedFilters=extend_cache,rewrite_javascript,trim_urls
FETCHED=$OUTDIR/blacklist.html
fetch_until $URL 'grep -c .js.pagespeed.' 4
$WGET_DUMP $URL > $FETCHED
check grep -q "<script src=\".*normal\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\"js_tinyMCE\.js\"></script>" $FETCHED
check grep -q "<script src=\"tiny_mce\.js\"></script>" $FETCHED
check grep -q "<script src=\"tinymce\.js\"></script>" $FETCHED
check grep -q \
  "<script src=\"scriptaculous\.js?load=effects,builder\"></script>" $FETCHED
check grep -q "<script src=\".*jquery.*\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\".*ckeditor\.js\">" $FETCHED
check grep -q "<script src=\".*swfobject\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q \
  "<script src=\".*another_normal\.js\.pagespeed\..*\.js\">" $FETCHED

WGET_ARGS=""
start_test move_css_above_scripts works.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=move_css_above_scripts
$WGET_DUMP $URL > $FETCHED
# Link moved before script.
check grep -q "styles/all_styles.css\"><script" $FETCHED

start_test move_css_above_scripts off.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved before script.
check_not grep "styles/all_styles.css\"><script" $FETCHED

start_test move_css_to_head does what it says on the tin.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=move_css_to_head
$WGET_DUMP $URL > $FETCHED
# Link moved to head.
check grep -q "styles/all_styles.css\"></head>" $FETCHED

start_test move_css_to_head off.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved to head.
check_not grep "styles/all_styles.css\"></head>" $FETCHED

test_filter inline_css converts 3 out of 5 link tags to style tags.
fetch_until $URL 'grep -c <style' 3

# In some test environments these tests can't be run because of
# restrictions on external connections
if [ -z ${DISABLE_FONT_API_TESTS:-} ]; then
  test_filter inline_google_font_css Can inline Google Font API loader CSS
  fetch_until $URL 'grep -c @font-face' 1

  # By default we use a Chrome UA, so it should get woff
  OUT=$($WGET_DUMP $URL)
  check_from "$OUT" fgrep -qi "format('woff')"
  check_not_from "$OUT" fgrep -qi "format('truetype')"
  check_not_from "$OUT" fgrep -qi "format('embedded-opentype')"

  # Now try with IE6 user-agent. We do this with setting WGETRC due to
  # quoting madness
  WGETRC_OLD=$WGETRC
  export WGETRC=$TEMPDIR/wgetrc-ie
  cat > $WGETRC <<EOF
user_agent = Mozilla/4.0 (compatible; MSIE 6.01; Windows NT 6.0)
EOF

  fetch_until $URL 'grep -c @font-face' 1
  # This should get an eot font. (It might also ship a woff, so we don't
  # check_not_from for that)
  OUT=$($WGET_DUMP $URL)
  check_from "$OUT" fgrep -qi "format('embedded-opentype')"
  check_not_from "$OUT" fgrep -qi "format('truetype')"
  export WGETRC=$WGETRC_OLD
fi

test_filter inline_javascript inlines a small JS file.
fetch_until $URL 'grep -c document.write' 1

test_filter outline_css outlines large styles, but not small ones.
check run_wget_with_args $URL
check egrep -q '<link.*text/css.*large' $FETCHED  # outlined
check egrep -q '<style.*small' $FETCHED           # not outlined

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined
start_test compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*.pagespeed.*.js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
check_from "$JS_HEADERS" egrep -qi 'HTTP/1[.]. 200 OK'
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
#check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
check_from "$JS_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'

test_filter pedantic adds default type attributes.
check run_wget_with_args $URL
check fgrep -q 'text/javascript' $FETCHED # should find script type
check fgrep -q 'text/css' $FETCHED        # should find style type


test_filter remove_comments removes comments but not IE directives.
check run_wget_with_args $URL
check_not grep removed $FETCHED   # comment, should not find
check grep -q preserved $FETCHED  # preserves IE directives

test_filter remove_quotes does what it says on the tin.
check run_wget_with_args $URL
num_quoted=$(sed 's/ /\n/g' $FETCHED | grep -c '"')
check [ $num_quoted -eq 2 ]  # 2 quoted attrs
num_apos=$(grep -c "'" $FETCHED)
check [ $num_apos -eq 0 ]    # no apostrophes

test_filter trim_urls makes urls relative
check run_wget_with_args $URL
check_not grep "mod_pagespeed_example" $FETCHED  # base dir, shouldn't find
check_file_size $FETCHED -lt 153  # down from 157

test_filter rewrite_css minifies CSS and saves bytes.
fetch_until -save $URL 'grep -c comment' 0
check_file_size $FETCH_FILE -lt 680  # down from 689

test_filter rewrite_images inlines, compresses, and resizes.
fetch_until $URL 'grep -c data:image/png' 1  # Images inlined.
fetch_until $URL 'grep -c .pagespeed.ic' 2  # Images rewritten.

# Verify with a blocking fetch that pagespeed_no_transform worked and was
# stripped.
fetch_until $URL 'grep -c "images/disclosure_open_plus.png"' 1 \
  '--header=X-PSA-Blocking-Rewrite:psatest'
fetch_until $URL 'grep -c "pagespeed_no_transform"' 0 \
  '--header=X-PSA-Blocking-Rewrite:psatest'

# Save successfully rewritten contents.
# Note: We cannot do this above because the intervening fetch_untils will
# clean up $OUTDIR.
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2
check_file_size "$WGET_DIR/xBikeCrashIcn*" -lt 25000      # re-encoded
check_file_size "$WGET_DIR/*256x192*Puzzle*" -lt 24126    # resized
URL=$EXAMPLE_ROOT"/rewrite_images.html?PageSpeedFilters=rewrite_images"

IMG_URL=$(egrep -o 'http://[^"]*pagespeed.[^"]*.jpg' $FETCHED | head -n1)
if [ -z "$IMG_URL" ]; then
  # If PreserveUrlRelativity is on, we need to find the relative URL and
  # absolutify it ourselves.
  IMG_URL="$EXAMPLE_ROOT/"
  IMG_URL+=$(grep -o '[^\"]*pagespeed.[^\"]*\.jpg' $FETCHED | head -n 1)
fi

start_test headers for rewritten image
echo "$IMG_URL"
IMG_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $IMG_URL 2>&1)
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
check_from "$IMG_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
# Make sure an extra header is propagated from input resource to output
# resource.  X-Extra-Header is added in debug.conf.template.
start_test Extra header is present
check_from "$IMG_HEADERS" fgrep -qi 'X-Extra-Header'
# Make sure there is a last-modified tag
start_test Last-modified is present.
check_from "$IMG_HEADERS" fgrep -qi 'Last-Modified'

IMAGES_QUALITY="PageSpeedImageRecompressionQuality"
JPEG_QUALITY="PageSpeedJpegRecompressionQuality"
WEBP_QUALITY="PageSpeedWebpRecompressionQuality"
start_test quality of jpeg output images with generic quality flag
URL="$TEST_ROOT/image_rewriting/rewrite_images.html"
WGET_ARGS="--header PageSpeedFilters:rewrite_images "
WGET_ARGS+="--header ${IMAGES_QUALITY}:75 "
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
WGET_ARGS=""
# This filter produces different images on 32 vs 64 bit builds. On 32 bit, the
# size is 8157B, while on 64 it is 8155B. Initial investigation showed no
# visible differences between the generated images.
# TODO(jmaessen) Verify that this behavior is expected.
#
# Note that if this test fails with 8251 it means that you have managed to get
# progressive jpeg conversion turned on in this testcase, which makes the output
# larger.  The threshold factor kJpegPixelToByteRatio in image_rewrite_filter.cc
# is tuned to avoid that.
check_file_size "$WGET_DIR/*256x192*Puzzle*" -le 8157   # resized

start_test quality of jpeg output images
URL="$TEST_ROOT/jpeg_rewriting/rewrite_images.html"
WGET_ARGS="--header PageSpeedFilters:rewrite_images "
WGET_ARGS+="--header ${IMAGES_QUALITY}:85 "
WGET_ARGS+="--header ${JPEG_QUALITY}:70"
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2   # 2 images optimized
WGET_ARGS=""
#
# If this this test fails because the image size is 7673 bytes it means
# that image_rewrite_filter.cc decided it was a good idea to convert to
# progressive jpeg, and in this case it's not.  See the not above on
# kJpegPixelToByteRatio.
check_file_size "$WGET_DIR/*256x192*Puzzle*" -le 7564   # resized

start_test quality of webp output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE="$TEST_ROOT/webp_rewriting/rewrite_images.html"
REWRITE_URL="$IMG_REWRITE?PageSpeedFilters=rewrite_images"
URL="$REWRITE_URL,convert_jpeg_to_webp&$IMAGES_QUALITY=75&$WEBP_QUALITY=65"
check run_wget_with_args \
  --header 'X-PSA-Blocking-Rewrite: psatest' --user-agent=webp $URL
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd
rm -rf $WGET_DIR
check run_wget_with_args \
  --header 'X-PSA-Blocking-Rewrite: psatest' --header='Accept: image/webp' $URL
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd

BAD_IMG_URL=$REWRITTEN_ROOT/images/xBadName.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
start_test rewrite_images fails broken image
echo run_wget_with_args $BAD_IMG_URL
run_wget_with_args $BAD_IMG_URL  # fails
check grep "404 Not Found" $WGET_OUTPUT

start_test "rewrite_images doesn't 500 on unoptomizable image."
IMG_URL=$REWRITTEN_ROOT/images/xOptPuzzle.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
run_wget_with_args $IMG_URL
check egrep "HTTP/1[.]. 200 OK" $WGET_OUTPUT

# These have to run after image_rewrite tests. Otherwise it causes some images
# to be loaded into memory before they should be.
WGET_ARGS=""
start_test rewrite_css,extend_cache extends cache of images in CSS.
FILE=rewrite_css_images.html?PageSpeedFilters=rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL

start_test fallback_rewrite_css_urls works.
FILE=fallback_rewrite_css_urls.html?\
PageSpeedFilters=fallback_rewrite_css_urls,rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until -save $URL 'grep -c fallback_rewrite_css_urls.css.pagespeed.cf.' 1
# Test this was fallback flow -> no minification.
check grep -q "body { background" $FETCH_FILE

# Rewrite images in styles.
start_test rewrite_images,rewrite_css,rewrite_style_attributes_with_url optimizes images in style.
FILE=rewrite_style_attributes.html?PageSpeedFilters=rewrite_images,rewrite_css,rewrite_style_attributes_with_url
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c BikeCrashIcn.png.pagespeed.ic.' 1
check run_wget_with_args $URL

# Now check that it can handle two of the same image in the same style block:
start_test two images in the same style block
FILE="rewrite_style_attributes_dual.html?PageSpeedFilters="
FILE+="rewrite_images,rewrite_css,rewrite_style_attributes_with_url"
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
PATTERN="BikeCrashIcn.png.pagespeed.ic.*BikeCrashIcn.png.pagespeed.ic"
fetch_until $URL "grep -c $PATTERN" 1
check run_wget_with_args $URL

start_test rewrite_css,rewrite_images rewrites and inlines images in CSS.
FILE='rewrite_css_images.html?PageSpeedFilters=rewrite_css,rewrite_images'
FILE+='&ModPagespeedCssImageInlineMaxBytes=2048'
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c url.data:image/png;base64,' 1  # image inlined
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL

start_test inline_css,rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?PageSpeedFilters=inline_css,rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
echo $WGET_DUMP $URL
fetch_until $URL \
  'grep -c Cuppa.png.*BikeCrashIcn.png.*IronChef2.gif.*.pagespeed.is.*.png' 1

start_test rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?PageSpeedFilters=rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until -save -recursive $URL 'grep -c css.pagespeed.cf' 1

# Extract out the rewritten CSS file from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file and look inside for the sprited image reference (ic.pagespeed.is...).
CSS=$(grep stylesheet "$WGET_DIR/$(basename $URL)" | cut -d\" -f 6)
echo css is $CSS
SPRITE_CSS_OUT="$WGET_DIR/$(basename $CSS)"
echo css file = $SPRITE_CSS_OUT
check [ $(grep -c "ic.pagespeed.is" "$SPRITE_CSS_OUT") -gt 0 ]

test_filter rewrite_javascript minifies JavaScript and saves bytes.
# External scripts rewritten.
fetch_until -save -recursive \
  $URL 'grep -c src=.*rewrite_javascript\.js\.pagespeed\.jm\.' 2
check_not grep removed $WGET_DIR/*.pagespeed.jm.*  # No comments should remain.
check_file_size $FETCH_FILE -lt 1560               # Net savings
check grep -q preserved $FETCH_FILE                # Preserves certain comments.
# Rewritten JS is cache-extended.
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

# Error path for fetch of outlined resources that are not in cache leaked
# at one point of development.
start_test regression test for RewriteDriver leak
$WGET -O /dev/null -o /dev/null $TEST_ROOT/_.pagespeed.jo.3tPymVdi9b.js

# Combination rewrite in which the same URL occurred twice used to
# lead to a large delay due to overly late lock release.
start_test regression test with same filtered input twice in combination
PAGE=_,Mco.0.css+_,Mco.0.css.pagespeed.cc.0.css
URL=$TEST_ROOT/$PAGE?PageSpeedFilters=combine_css,outline_css
echo $WGET -O /dev/null -o /dev/null --tries=1 --read-timeout=3 $URL
$WGET -O /dev/null -o /dev/null --tries=1 --read-timeout=3 $URL
# We want status code 8 (server-issued error) and not 4
# (network failure/timeout)
check [ $? = 8 ]

WGET_ARGS=""

# Simple test that https is working.
if [ -n "$HTTPS_HOST" ]; then
  URL="$HTTPS_EXAMPLE_ROOT/combine_css.html"
  fetch_until $URL 'fgrep -c css+' 1 --no-check-certificate

  start_test https is working.
  echo $WGET_DUMP_HTTPS $URL
  HTML_HEADERS=$($WGET_DUMP_HTTPS $URL)

  echo Checking for X-Mod-Pagespeed header
  check_from "$HTML_HEADERS" egrep -q 'X-Mod-Pagespeed|X-Page-Speed'

  echo Checking for combined CSS URL
  EXPECTED='href="styles/yellow\.css+blue\.css+big\.css+bold\.css'
  EXPECTED="$EXPECTED"'\.pagespeed\.cc\..*\.css"/>'
  fetch_until "$URL?PageSpeedFilters=combine_css,trim_urls" \
      "grep -ic $EXPECTED" 1 --no-check-certificate

  echo Checking for combined CSS URL without URL trimming
  # Without URL trimming we still preserve URL relativity.
  fetch_until "$URL?PageSpeedFilters=combine_css" "grep -ic $EXPECTED" 1 \
     --no-check-certificate
fi

# This filter convert the meta tags in the html into headers.
test_filter convert_meta_tags
run_wget_with_args $URL

echo Checking for Charset header.
check grep -qi "CONTENT-TYPE: text/html; *charset=UTF-8" $WGET_OUTPUT

# This filter loads below the fold images lazily.
test_filter lazyload_images
check run_wget_with_args $URL
# Check src gets swapped with pagespeed_lazy_src
check fgrep -q "pagespeed_lazy_src=\"images/Puzzle.jpg\"" $FETCHED
check fgrep -q "pagespeed.lazyLoadInit" $FETCHED  # inline script injected

echo Testing whether we can rewrite javascript resources that are served
echo gzipped, even though we generally ask for them clear.  This particular
echo js file has "alert('Hello')" but is checked into source control in gzipped
echo format and served with the gzip headers, so it is decodable.  This tests
echo that we can inline and minify that file.
test_filter rewrite_javascript,inline_javascript with gzipped js origin
URL="$TEST_ROOT/rewrite_compressed_js.html"
QPARAMS="PageSpeedFilters=rewrite_javascript,inline_javascript"
fetch_until "$URL?$QPARAMS" "grep -c Hello'" 1

echo Test that we can rewrite resources that are served with
echo Cache-Control: no-cache with on-the-fly filters.  Tests that the
echo no-cache header is preserved.
test_filter extend_cache with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.ce.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.ce.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

echo Test that we can rewrite Cache-Control: no-cache resources with
echo non-on-the-fly filters.
test_filter rewrite_javascript with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.jm.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.jm.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

start_test ?PageSpeed=noscript inserts canonical href link
OUT=$($WGET_DUMP $EXAMPLE_ROOT/defer_javascript.html?PageSpeed=noscript)
check_from "$OUT" egrep -q \
  "link rel=\"canonical\" href=\"$EXAMPLE_ROOT/defer_javascript.html\""

# Checks that defer_javascript injects 'pagespeed.deferJs' from defer_js.js,
# but strips the comments.
test_filter defer_javascript optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q text/psajs $FETCHED
check grep -q /js_defer $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that defer_javascript,debug injects 'pagespeed.deferJs' from
# defer_js.js, but retains the comments.
test_filter defer_javascript,debug optimize mode
FILE=defer_javascript.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args "$URL"
check grep -q text/psajs $FETCHED
check grep -q /js_defer_debug $FETCHED
# The deferjs src url is in the format js_defer.<hash>.js. This strips out
# everthing except the js filename and saves it to test fetching later.
DEFERJSURL=`grep js_defer $FETCHED | sed 's/^.*js_defer/js_defer/;s/\.js.*$/\.js/g;'`
check grep -q "PageSpeed=noscript" $FETCHED

# Extract out the DeferJs url from the HTML above and fetch it.
start_test Fetch the deferJs url with hash.
echo run_wget_with_args $DEFERJSURL
run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/$DEFERJSURL
check fgrep "200 OK" $WGET_OUTPUT
check fgrep "Cache-Control: max-age=31536000" $WGET_OUTPUT

# Checks that we return 404 for static file request without hash.
start_test Access to js_defer.js without hash returns 404.
echo run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.js
run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.js
check fgrep "404 Not Found" $WGET_OUTPUT

# Checks that outlined js_defer.js is served correctly.
start_test serve js_defer.0.js
echo run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.0.js
run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer.0.js
check fgrep "200 OK" $WGET_OUTPUT
check fgrep "Cache-Control: max-age=300,private" $WGET_OUTPUT

# Checks that outlined js_defer_debug.js is  served correctly.
start_test serve js_defer_debug.0.js
echo run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer_debug.0.js
run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/js_defer_debug.0.js
check fgrep "200 OK" $WGET_OUTPUT
check fgrep "Cache-Control: max-age=300,private" $WGET_OUTPUT

# Checks that lazyload_images injects compiled javascript from
# lazyload_images.js.
test_filter lazyload_images optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q pagespeed.lazyLoad $FETCHED
check_not grep '/\*' $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED
# The lazyload placeholder image is in the format 1.<hash>.gif. This matches the
# first src attribute set to the placeholder, and then strips out everything
# except for the gif name for later testing of fetching this image.
BLANKGIFSRC=`grep -m1 -o " src=.*1.*.gif" $FETCHED | sed 's/^.*1\./1./;s/\.gif.*$/\.gif/g'`

# Fetch the blank image and make sure it's served correctly.
start_test serve_blank_gif
echo run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/$BLANKGIFSRC
run_wget_with_args http://$PROXY_DOMAIN/$PSA_JS_LIBRARY_URL_PREFIX/$BLANKGIFSRC
check fgrep "200 OK" $WGET_OUTPUT
check fgrep "Cache-Control: max-age=31536000" $WGET_OUTPUT

# Checks that lazyload_images,debug injects non-optimized javascript from
# lazyload_images.js. The debug JS will still have comments stripped, since we
# run it through the closure compiler to resolve any uses of goog.require.
test_filter lazyload_images,debug debug mode
FILE=lazyload_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args "$URL"
check grep -q pagespeed.lazyLoad $FETCHED
check_not grep -q '/\*' $FETCHED
check_not grep -q 'goog.require' $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that inline_preview_images injects compiled javascript
test_filter inline_preview_images optimize mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
echo run_wget_with_args $URL
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 1
fetch_until $URL 'grep -c /\*' 0
check run_wget_with_args $URL

# Checks that inline_preview_images,debug injects from javascript
# in non-compiled mode
test_filter inline_preview_images,debug debug mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 3
check run_wget_with_args $URL

# Checks that local_storage_cache injects optimized javascript from
# local_storage_cache.js, adds the pagespeed_lsc_ attributes, inlines the data
# (if the cache were empty the inlining wouldn't make the timer cutoff but the
# resources have been fetched above).
test_filter local_storage_cache,inline_css,inline_images optimize mode
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args "$URL"
check run_wget_with_args "$URL"
check grep -q "pagespeed.localStorageCacheInit()" $FETCHED
check [ $(grep -c ' pagespeed_lsc_url=' $FETCHED) = 2 ]
check grep -q "yellow {background-color: yellow" $FETCHED
check grep -q "<img src=\"data:image/png;base64" $FETCHED
check grep -q "<img .* alt=\"A cup of joe\"" $FETCHED
check_not grep -q "/\*" $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that local_storage_cache,debug injects debug javascript from
# local_storage_cache.js, adds the pagespeed_lsc_ attributes, inlines the data
# (if the cache were empty the inlining wouldn't make the timer cutoff but the
# resources have been fetched above).
test_filter local_storage_cache,inline_css,inline_images,debug debug mode
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args "$URL"
check run_wget_with_args "$URL"
check grep -q "pagespeed.localStorageCacheInit()" $FETCHED
check [ $(grep -c ' pagespeed_lsc_url=' $FETCHED) = 2 ]
check grep -q "yellow {background-color: yellow" $FETCHED
check grep -q "<img src=\"data:image/png;base64" $FETCHED
check grep -q "<img .* alt=\"A cup of joe\"" $FETCHED
check_not grep -q "/\*" $FETCHED
check_not grep -q "goog.require" $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that local_storage_cache doesn't send the inlined data for a resource
# whose hash is in the magic cookie. First get the cookies from prior runs.
HASHES=$(grep "pagespeed_lsc_hash=" $FETCHED |\
         sed -e 's/^.*pagespeed_lsc_hash=.//' |\
         sed -e 's/".*$//')
HASHES=$(echo "$HASHES" | tr '\n' '!' | sed -e 's/!$//')
check [ -n "$HASHES" ]
COOKIE="Cookie: _GPSLSC=$HASHES"
# Check that the prior run did inline the data.
check grep -q "background-color: yellow" $FETCHED
check grep -q "src=.data:image/png;base64," $FETCHED
check grep -q "alt=.A cup of joe." $FETCHED
# Fetch with the cookie set.
test_filter local_storage_cache,inline_css,inline_images cookies set
check run_wget_with_args --save-headers --no-cookies --header "$COOKIE" $URL
# Check that this run did NOT inline the data.
check_not fgrep "yellow {background-color: yellow" $FETCHED
check_not grep "src=.data:image/png;base64," $FETCHED
# Check that this run inserted the expected scripts.
check grep -q \
  "pagespeed.localStorageCache.inlineCss(.http://.*/styles/yellow.css.);" \
  $FETCHED
check grep -q \
  "pagespeed.localStorageCache.inlineImg(.http://.*/images/Cuppa.png.," \
  ".alt=A cup of joe.," \
  ".alt=A cup of joe.," \
  ".alt=A cup of joe.s ..joe...," \
  ".alt=A cup of joe.s ..joe...);" \
  $FETCHED

# Test flatten_css_imports.

# Fetch with the default limit so our test file is inlined.
test_filter flatten_css_imports,rewrite_css default limit
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check_not grep @import.url $FETCHED
check grep -q "yellow.background-color:" $FETCHED

# Fetch with a tiny limit so no file can be inlined.
test_filter flatten_css_imports,rewrite_css tiny limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:5"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Fetch with a medium limit so any one file can be inlined but not all of them.
test_filter flatten_css_imports,rewrite_css medium limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:50"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Cache extend PDFs.
test_filter extend_cache_pdfs PDF cache extension
WGET_EC="$WGET_DUMP $WGET_ARGS"

start_test Html is rewritten with cache-extended PDFs.
fetch_until -save $URL 'fgrep -c .pagespeed.' 3
check grep -q 'a href=".*pagespeed.*\.pdf' $FETCH_FILE
check grep -q 'embed src=".*pagespeed.*\.pdf' $FETCH_FILE
check fgrep -q '<a href="example.notpdf">' $FETCH_FILE
check grep -q '<a href=".*pagespeed.*\.pdf">example.pdf?a=b' $FETCH_FILE

start_test Cache-extended PDFs load and have the right mime type.
PDF_CE_URL=$(grep -o 'http://[^\"]*pagespeed.[^\"]*\.pdf' $FETCH_FILE | \
             head -n 1)
if [ -z "$PDF_CE_URL" ]; then
  # If PreserveUrlRelativity is on, we need to find the relative URL and
  # absolutify it ourselves.
  PDF_CE_URL="$EXAMPLE_ROOT/"
  PDF_CE_URL+=$(grep -o '[^\"]*pagespeed.[^\"]*\.pdf' $FETCH_FILE | head -n 1)
fi
echo Extracted cache-extended url $PDF_CE_URL
OUT=$($WGET_EC $PDF_CE_URL)
check_from "$OUT" grep -aq 'Content-Type: application/pdf'

# Test DNS prefetching. DNS prefetching is dependent on user agent, but is
# enabled for Wget UAs, allowing this test to work with our default wget params.
test_filter insert_dns_prefetch
fetch_until $URL 'fgrep -ci //ref.pssdemos.com' 2
fetch_until $URL 'fgrep -ci //ajax.googleapis.com' 2

# Test dedup_inlined_images
test_filter dedup_inlined_images,inline_images
fetch_until -save $URL 'fgrep -ocw inlineImg(' 4
check grep -q "PageSpeed=noscript" $FETCH_FILE

# Make sure we don't blank url(data:...) in CSS.
start_test CSS data URLs
URL=$REWRITTEN_ROOT/styles/A.data.css.pagespeed.cf.Hash.css
OUT=$($WGET_DUMP $URL)
check_from "$OUT" fgrep -q 'data:image/png'

# Cleanup
rm -rf $OUTDIR

# TODO(jefftk): Find out what test breaks without the next two lines and fix it.
filter_spec_method="query_params"
test_filter '' Null Filter
