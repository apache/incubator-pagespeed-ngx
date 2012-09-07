#!/bin/bash
# Copyright 2010 Google Inc. All Rights Reserved.
# Author: abliss@google.com (Adam Bliss)
#
# Generic system test, which should work on any implementation of
# Page Speed Automatic (not just the Apache module).
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.

# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir="$( dirname "${BASH_SOURCE[0]}" )"
source "$this_dir/system_test_helpers.sh" || exit 1

# General system tests

echo TEST: Page Speed Automatic is running and writes the expected header.
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

# TODO(sligocki): We should have Expires headers in HTML just like resources.
#echo Checking for absence of Expires
#check_not fgrep -i 'Expires' $HTTP_FILE

echo Checking for absence of X-Frame-Options: SAMEORIGIN
check_not fgrep -i "X-Frame-Options" $HTTP_FILE

# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?ModPagespeed=off.
echo TEST: directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q $EXAMPLE_ROOT/?ModPagespeed=off -O $OUTDIR/mod_pagespeed_example
check $WGET -q $EXAMPLE_ROOT/index.html?ModPagespeed=off -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example

echo TEST: compression is enabled for HTML.
check fgrep -qi 'Content-Encoding: gzip' <(
  $WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' $EXAMPLE_ROOT/ 2>&1)

echo TEST: X-Mod-Pagespeed header added when ModPagespeed=on
check egrep -q 'X-Mod-Pagespeed|X-Page-Speed' <(
  $WGET_DUMP $EXAMPLE_ROOT/combine_css.html?ModPagespeed=on)

echo TEST: X-Mod-Pagespeed header not added when ModPagespeed=off
check_not egrep 'X-Mod-Pagespeed|X-Page-Speed' <(
  $WGET_DUMP $EXAMPLE_ROOT/combine_css.html?ModPagespeed=off)

echo TEST: We behave sanely on whitespace served as HTML
check egrep -q 'HTTP/1[.]. 200 OK' <($WGET_DUMP $TEST_ROOT/whitespace.html)

# Individual filter tests, in alphabetical order

test_filter add_instrumentation adds 2 script tags
check run_wget_with_args $URL
# Counts occurances of '<script' in $FETCHED
# See: http://superuser.com/questions/339522
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 2 ]

echo "TEST: We don't add_instrumentation if URL params tell us not to"
FILE=add_instrumentation.html?ModPagespeedFilters=
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
check run_wget_with_args $URL
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 0 ]

# http://code.google.com/p/modpagespeed/issues/detail?id=170
echo "TEST: Make sure 404s aren't rewritten"
# Note: We run this in the add_instrumentation section because that is the
# easiest to detect which changes every page
THIS_BAD_URL=$BAD_RESOURCE_URL?ModPagespeedFilters=add_instrumentation
# We use curl, because wget does not save 404 contents
check_not fgrep "/mod_pagespeed_beacon" <($CURL --silent $THIS_BAD_URL)

test_filter collapse_whitespace removes whitespace, but not from pre tags.
check run_wget_with_args $URL
check [ $(egrep -c '^ +<' $FETCHED) -eq 1 ]

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'fgrep -c text/css' 1
check run_wget_with_args $URL
#test_resource_ext_corruption $URL $combine_css_filename

echo TEST: combine_css without hash field should 404
echo run_wget_with_args $REWRITTEN_ROOT/styles/yellow.css+blue.css.pagespeed.cc..css
run_wget_with_args $REWRITTEN_ROOT/styles/yellow.css+blue.css.pagespeed.cc..css
check fgrep "404 Not Found" $WGET_OUTPUT

# Note: this large URL can only be processed by Apache if
# ap_hook_map_to_storage is called to bypass the default
# handler that maps URLs to filenames.
echo TEST: Fetch large css_combine URL
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
check egrep -q "HTTP/1[.]. 200 OK" <(
  $WGET --save-headers -q -O - $LARGE_URL | head -1)
LARGE_URL_LINE_COUNT=$($WGET -q -O - $LARGE_URL | wc -l)
echo Checking that response body is at least 900 lines -- it should be 954
check [ $LARGE_URL_LINE_COUNT -gt 900 ]

test_filter combine_javascript combines 2 JS files into 1.
fetch_until $URL 'fgrep -c src=' 1
check run_wget_with_args $URL

echo TEST: combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?ModPagespeedFilters=combine_javascript
fetch_until $URL 'fgrep -c src=' 4

test_filter combine_heads combines 2 heads into 1
check run_wget_with_args $URL
check [ $(fgrep -c '<head>' $FETCHED) = 1 ]

test_filter elide_attributes removes boolean and default attributes.
check run_wget_with_args $URL
check_not fgrep "disabled=" $FETCHED   # boolean, should not find
check_not fgrep "type=" $FETCHED       # default, should not find

test_filter extend_cache_images rewrites an image tag.
URL=$EXAMPLE_ROOT/extend_cache.html?ModPagespeedFilters=extend_cache_images
fetch_until $URL 'egrep -c src.*/Puzzle[.]jpg[.]pagespeed[.]ce[.].*[.]jpg' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
#test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

echo TEST: Attempt to fetch cache-extended image without hash should 404
run_wget_with_args $REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce..jpg
check fgrep "404 Not Found" $WGET_OUTPUT

echo TEST: Cache-extended image should respond 304 to an If-Modified-Since.
URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
DATE=$(date -R)
run_wget_with_args --header "If-Modified-Since: $DATE" $URL
check fgrep "304 Not Modified" $WGET_OUTPUT

echo TEST: Legacy format URLs should still work.
URL=$REWRITTEN_ROOT/images/ce.0123456789abcdef0123456789abcdef.Puzzle,j.jpg
# Note: Wget request is HTTP/1.0, so some servers respond back with
# HTTP/1.0 and some respond back 1.1.
$WGET_DUMP $URL > $FETCHED
check egrep -q 'HTTP/1[.]. 200 OK' $FETCHED

echo TEST: Filters do not rewrite blacklisted JavaScript files.
URL=$TEST_ROOT/blacklist/blacklist.html?ModPagespeedFilters=extend_cache,rewrite_javascript,trim_urls
FETCHED=$OUTDIR/blacklist.html
fetch_until $URL 'grep -c .js.pagespeed.' 4
$WGET_DUMP $URL > $FETCHED
cat $FETCHED
check grep "<script src=\".*normal\.js\.pagespeed\..*\.js\">" $FETCHED
check grep "<script src=\"js_tinyMCE\.js\"></script>" $FETCHED
check grep "<script src=\"tiny_mce\.js\"></script>" $FETCHED
check grep "<script src=\"tinymce\.js\"></script>" $FETCHED
check grep "<script src=\"scriptaculous\.js?load=effects,builder\"></script>" \
  $FETCHED
check grep "<script src=\"connect\.facebook\.net/en_US/all\.js\"></script>" \
  $FETCHED
check grep "<script src=\".*jquery.*\.js\.pagespeed\..*\.js\">" $FETCHED
check grep "<script src=\".*ckeditor\.js\">" $FETCHED
check grep "<script src=\".*swfobject\.js\.pagespeed\..*\.js\">" $FETCHED
check grep "<script src=\".*another_normal\.js\.pagespeed\..*\.js\">" $FETCHED

WGET_ARGS=""
echo TEST: move_css_above_scripts works.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?ModPagespeedFilters=move_css_above_scripts
$WGET_DUMP $URL > $FETCHED
# Link moved before script.
cat $FETCHED
check grep -q "styles/all_styles.css\"><script" $FETCHED

echo TEST: move_css_above_scripts off.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?ModPagespeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved before script.
check_not grep "styles/all_styles.css\"><script" $FETCHED

echo TEST: move_css_to_head does what it says on the tin.
URL=$EXAMPLE_ROOT/move_css_to_head.html?ModPagespeedFilters=move_css_to_head
$WGET_DUMP $URL > $FETCHED
# Link moved to head.
check grep -q "styles/all_styles.css\"></head>" $FETCHED

echo TEST: move_css_to_head off.
URL=$EXAMPLE_ROOT/move_css_to_head.html?ModPagespeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved to head.
check_not grep "styles/all_styles.css\"></head>" $FETCHED

test_filter inline_css converts 3 out of 5 link tags to style tags.
fetch_until $URL 'grep -c <style' 3

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
echo TEST: compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*.pagespeed.*.js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
echo JS_HEADERS=$JS_HEADERS
check egrep -qi 'HTTP/1[.]. 200 OK' <(echo $JS_HEADERS)
check fgrep -qi 'Content-Encoding: gzip' <(echo $JS_HEADERS)
#check fgrep -qi 'Vary: Accept-Encoding' <(echo $JS_HEADERS)
fgrep -qi 'Etag: W/0' <(echo $JS_HEADERS)
check fgrep -qi 'Last-Modified:' <(echo $JS_HEADERS)

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
check [ $(stat -c %s $FETCHED) -lt 153 ]  # down from 157

test_filter rewrite_css minifies CSS and saves bytes.
fetch_until $URL 'grep -c comment' 0
check run_wget_with_args $URL
check [ $(stat -c %s $FETCHED) -lt 680 ]  # down from 689

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
check egrep -qi 'HTTP/1[.]. 200 OK' <(echo $IMG_HEADERS)
# Make sure we have some valid headers.
check fgrep -qi 'Content-Type: image/jpeg' <(echo "$IMG_HEADERS")
# Make sure the response was not gzipped.
echo TEST: Images are not gzipped.
check_not fgrep -i 'Content-Encoding: gzip' <(echo "$IMG_HEADERS")
# Make sure there is no vary-encoding
echo TEST: Vary is not set for images.
check_not fgrep -i 'Vary: Accept-Encoding' <(echo "$IMG_HEADERS")
# Make sure there is an etag
echo TEST: Etags is present.
check fgrep -qi 'Etag: W/0' <(echo "$IMG_HEADERS")
# TODO(sligocki): Allow setting arbitrary headers in static_server.
# Make sure an extra header is propagated from input resource to output
# resource.  X-Extra-Header is added in debug.conf.template.
#echo TEST: Extra header is present
#check fgrep -qi 'X-Extra-Header' <(echo "$IMG_HEADERS")
# Make sure there is a last-modified tag
echo TEST: Last-modified is present.
check fgrep -qi 'Last-Modified' <(echo "$IMG_HEADERS")

BAD_IMG_URL=$REWRITTEN_ROOT/images/xBadName.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
echo "TEST: rewrite_images fails broken image \"$BAD_IMG_URL\"."
echo run_wget_with_args $BAD_IMG_URL
run_wget_with_args $BAD_IMG_URL  # fails
check grep "404 Not Found" $WGET_OUTPUT

echo "TEST: rewrite_images doesn't 500 on unoptomizable image."
IMG_URL=$REWRITTEN_ROOT/images/xOptPuzzle.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
run_wget_with_args $IMG_URL
check egrep "HTTP/1[.]. 200 OK" $WGET_OUTPUT

# These have to run after image_rewrite tests. Otherwise it causes some images
# to be loaded into memory before they should be.
WGET_ARGS=""
echo TEST: rewrite_css,extend_cache extends cache of images in CSS.
FILE=rewrite_css_images.html?ModPagespeedFilters=rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL

echo TEST: fallback_rewrite_css_urls works.
FILE=fallback_rewrite_css_urls.html?\
ModPagespeedFilters=fallback_rewrite_css_urls,rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until $URL 'grep -c fallback_rewrite_css_urls.css.pagespeed.cf.' 1
check run_wget_with_args $URL
# Test this was fallback flow -> no minification.
check grep -q "body { background" $FETCHED

# Rewrite images in styles.
echo TEST: rewrite_images,rewrite_css,rewrite_style_attributes_with_url optimizes images in style.
FILE=rewrite_style_attributes.html?ModPagespeedFilters=rewrite_images,rewrite_css,rewrite_style_attributes_with_url
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
fetch_until $URL 'grep -c BikeCrashIcn.png.pagespeed.ic.' 1
check run_wget_with_args $URL

echo TEST: rewrite_css,rewrite_images rewrites images in CSS.
FILE=rewrite_css_images.html?ModPagespeedFilters=rewrite_css,rewrite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
fetch_until $URL 'grep -c url.data:image/png;base64,' 1  # image inlined
fetch_until $URL 'grep -c rewrite_css_images.css.pagespeed.cf.' 1
check run_wget_with_args $URL

echo TEST: inline_css,rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?ModPagespeedFilters=inline_css,rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
echo $WGET_DUMP $URL
fetch_until $URL \
'grep -c Cuppa.png.*BikeCrashIcn.png.*IronChef2.gif.*.pagespeed.is.*.png' 1

echo TEST: rewrite_css,sprite_images sprites images in CSS.
FILE=sprite_images.html?ModPagespeedFilters=rewrite_css,sprite_images
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
echo $WGET_DUMP $URL
fetch_until $URL 'grep -c css.pagespeed.cf' 1
echo $WGET_DUMP $URL
$WGET_DUMP $URL > $OUTDIR/sprite_output
CSS=$(grep stylesheet $OUTDIR/sprite_output | cut -d\" -f 6)
echo css is $CSS
$WGET_DUMP $CSS > $OUTDIR/sprite_css_output
cat $OUTDIR/sprite_css_output
echo ""
check [ $(grep -c "ic.pagespeed.is" $OUTDIR/sprite_css_output) -gt 0 ]

test_filter rewrite_javascript minifies JavaScript and saves bytes.
# External scripts rewritten.
fetch_until $URL 'grep -c src.*/rewrite_javascript\.js\.pagespeed\.jm\.' 2
check run_wget_with_args $URL
check_not grep -R "removed" $OUTDIR          # Comments, should not find any.
check [ "$(stat -c %s $FETCHED)" -lt 1560 ]  # Net savings
check grep -q preserved $FETCHED             # Preserves certain comments.
# Rewritten JS is cache-extended.
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

# Error path for fetch of outlined resources that are not in cache leaked
# at one point of development.
echo TEST: regression test for RewriteDriver leak
$WGET -O /dev/null -o /dev/null $TEST_ROOT/_.pagespeed.jo.3tPymVdi9b.js

# Combination rewrite in which the same URL occurred twice used to
# lead to a large delay due to overly late lock release.
echo TEST: regression test with same filtered input twice in combination
PAGE=_,Mco.0.css+_,Mco.0.css.pagespeed.cc.0.css
URL=$TEST_ROOT/$PAGE?ModPagespeedFilters=combine_css,outline_css
echo $WGET -O /dev/null -o /dev/null --tries=1 --read-timeout=3 $URL
$WGET -O /dev/null -o /dev/null --tries=1 --read-timeout=3 $URL
# We want status code 8 (server-issued error) and not 4
# (network failure/timeout)
check [ $? = 8 ]

WGET_ARGS=""

# Simple test that https is working.
if [ -n "$HTTPS_HOST" ]; then
  URL="$HTTPS_EXAMPLE_ROOT/combine_css.html"
  fetch_until $URL 'grep -c css+' 1 --no-check-certificate

  echo TEST: https is working.
  echo $WGET_DUMP_HTTPS $URL
  HTML_HEADERS=$($WGET_DUMP_HTTPS $URL)

  echo Checking for X-Mod-Pagespeed header
  check egrep -q 'X-Mod-Pagespeed|X-Page-Speed' <(echo $HTML_HEADERS)

  echo Checking for combined CSS URL
  EXPECTED='href="styles/yellow\.css+blue\.css+big\.css+bold\.css'
  EXPECTED="$EXPECTED"'\.pagespeed\.cc\..*\.css"/>'
  fetch_until "$URL?ModPagespeedFilters=combine_css,trim_urls" \
      "grep -ic $EXPECTED" 1

  echo Checking for combined CSS URL without URL trimming
  EXPECTED="href=\"$HTTPS_EXAMPLE_ROOT/"
  EXPECTED="$EXPECTED"'styles/yellow\.css+blue\.css+big\.css+bold\.css'
  EXPECTED="$EXPECTED"'\.pagespeed\.cc\..*\.css"/>'
  fetch_until "$URL?ModPagespeedFilters=combine_css" "grep -ic $EXPECTED" 1
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
QPARAMS="ModPagespeedFilters=rewrite_javascript,inline_javascript"
fetch_until "$URL?$QPARAMS" "grep -c Hello'" 1

echo Test that we can rewrite resources that are served with
echo Cache-Control: no-cache with on-the-fly filters.  Tests that the
echo no-cache header is preserved.
test_filter extend_cache with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.ce.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
cat $WGET_OUTPUT
cat $OUTDIR/hello.js.pagespeed.ce.0.js
check fgrep -q "'Hello'" $OUTDIR/hello.js.pagespeed.ce.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

echo Test that we can rewrite Cache-Control: no-cache resources with
echo non-on-the-fly filters.
test_filter rewrite_javascript with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.jm.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $OUTDIR/hello.js.pagespeed.jm.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

echo TEST: ?ModPagespeed=noscript inserts canonical href link
check egrep -q \
  "link rel=\"canonical\" href=\"$EXAMPLE_ROOT/defer_javascript.html\"" <(
  $WGET_DUMP $EXAMPLE_ROOT/defer_javascript.html?ModPagespeed=noscript)

# Checks that defer_javascript injects 'pagespeed.deferJs' from defer_js.js,
# but strips the comments.
test_filter defer_javascript optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q pagespeed.deferJs $FETCHED
check grep -q text/psajs $FETCHED
check_not grep '/\*' $FETCHED
check grep -q "ModPagespeed=noscript" $FETCHED

# Checks that defer_javascript,debug injects 'pagespeed.deferJs' from
# defer_js.js, but retains the comments.
test_filter defer_javascript,debug optimize mode
FILE=defer_javascript.html?ModPagespeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
check run_wget_with_args "$URL"
check grep -q pagespeed.deferJs $FETCHED
check grep -q text/psajs $FETCHED
check grep -q '/\*' $FETCHED
check grep -q "ModPagespeed=noscript" $FETCHED

# Checks that lazyload_images injects compiled javascript from
# lazyload_images.js.
test_filter lazyload_images optimize mode
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q pagespeed.lazyLoad $FETCHED
check_not grep '/\*' $FETCHED
check grep -q "ModPagespeed=noscript" $FETCHED

# Checks that lazyload_images,debug injects non compiled javascript from
# lazyload_images.js
test_filter lazyload_images,debug debug mode
FILE=lazyload_images.html?ModPagespeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
check run_wget_with_args "$URL"
check grep -q pagespeed.lazyLoad $FETCHED
check grep -q '/\*' $FETCHED
check grep -q "ModPagespeed=noscript" $FETCHED

# Checks that inline_preview_images injects compiled javascript
test_filter inline_preview_images optimize mode
FILE=delay_images.html?ModPagespeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
echo run_wget_with_args $URL
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 1
fetch_until $URL 'grep -c /\*' 0
check run_wget_with_args $URL

# Checks that inline_preview_images,debug injects from javascript
# in non-compiled mode
test_filter inline_preview_images,debug debug mode
FILE=delay_images.html?ModPagespeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$OUTDIR/$FILE
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
check grep -q "ModPagespeed=noscript" $FETCHED

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
check grep -q "/\*" $FETCHED
check grep -q "ModPagespeed=noscript" $FETCHED

# Checks that local_storage_cache doesn't send the inlined data for a resource
# whose hash is in the magic cookie. First get the cookies from prior runs.
HASHES=$(grep "pagespeed_lsc_hash=" $FETCHED |\
         sed -e 's/^.*pagespeed_lsc_hash=.//' |\
         sed -e 's/".*$//')
HASHES=$(echo "$HASHES" | tr '\n' ',' | sed -e 's/,$//')
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
WGET_ARGS="${WGET_ARGS} --header=ModPagespeedCssFlattenMaxBytes:5"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Fetch with a medium limit so any one file can be inlined but not all of them.
test_filter flatten_css_imports,rewrite_css medium limit
WGET_ARGS="${WGET_ARGS} --header=ModPagespeedCssFlattenMaxBytes:50"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Cache extend PDFs.
test_filter extend_cache_pdfs PDF cache extension
WGET_EC="$WGET_DUMP $WGET_ARGS"

echo TEST: Html is rewritten with cache-extended PDFs.
fetch_until $URL 'fgrep -c .pagespeed.' 3

check grep 'a href="http://.*pagespeed.*\.pdf' <($WGET_EC $URL)
check grep 'embed src="http://.*pagespeed.*\.pdf' <($WGET_EC $URL)
check fgrep '<a href="example.notpdf">' <($WGET_EC $URL)
check grep 'a href="http://.*pagespeed.*\.pdf?a=b' <($WGET_EC $URL)

echo TEST: Cache-extended PDFs load and have the right mime type.
PDF_CE_URL="$($WGET_EC $URL | \
              grep -o 'http://.*pagespeed.[^\"]*\.pdf' | head -n 1)"
echo Extracted cache-extended url $PDF_CE_URL
check grep -a 'Content-Type: application/pdf' <($WGET_EC $PDF_CE_URL)

# Cleanup
rm -rf $OUTDIR

# TODO(jefftk): Find out what test breaks without the next two lines and fix it.
filter_spec_method="query_params"
test_filter '' Null Filter

echo "PASS."
