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
test_filter extend_cache_images rewrites an image tag.
URL=$EXAMPLE_ROOT/extend_cache.html?PageSpeedFilters=extend_cache_images
fetch_until $URL 'egrep -c src.*/Puzzle[.]jpg[.]pagespeed[.]ce[.].*[.]jpg' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
#test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

start_test Attempt to fetch cache-extended image without hash should 404
check_not run_wget_with_args $REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce..jpg
check fgrep "404 Not Found" $WGET_OUTPUT

start_test Cache-extended image should respond 304 to an If-Modified-Since.
URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
DATE=$(date -R)
check_not run_wget_with_args --header "If-Modified-Since: $DATE" $URL
check fgrep "304 Not Modified" $WGET_OUTPUT

start_test Cache-extended last-modified date should match origin
URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
ORIGIN_URL=$REWRITTEN_ROOT/images/Puzzle.jpg\?PageSpeed=off
rm -f $WGET_OUTPUT
$WGET_DUMP $ORIGIN_URL > $WGET_OUTPUT
echo $WGET_DUMP $ORIGIN_URL '>' $WGET_OUTPUT
origin_last_modified=$(extract_headers $WGET_OUTPUT | \
  scrape_header Last-Modified)
check [ "$origin_last_modified" != '' ];
rm -f $WGET_OUTPUT
$WGET_DUMP $URL > $WGET_OUTPUT
extended_last_modified=$(extract_headers $WGET_OUTPUT | \
  scrape_header Last-Modified)
check [ "$origin_last_modified" = "$extended_last_modified" ]

start_test Legacy format URLs should still work.
URL=$REWRITTEN_ROOT/images/ce.0123456789abcdef0123456789abcdef.Puzzle,j.jpg
# Note: Wget request is HTTP/1.0, so some servers respond back with
# HTTP/1.0 and some respond back 1.1.
$WGET_DUMP $URL > $FETCHED
check_200_http_response_file "$FETCHED"

# Cache extend PDFs.
test_filter extend_cache_pdfs PDF cache extension

# Saves any WGET_ARGS computed by 'test_filter' into WGET_EC, past the
# invocation of start_test Cache-extended PDFs below, which clears WGET_ARGS.
WGET_EC="$WGET_DUMP $WGET_ARGS"

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
