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

start_test X-Mod-Pagespeed header added when PageSpeed=on
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html?PageSpeed=on)
check_from "$OUT" egrep -q 'X-Mod-Pagespeed|X-Page-Speed'

start_test X-Mod-Pagespeed header not added when PageSpeed=off
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html?PageSpeed=off)
check_not_from "$OUT" egrep 'X-Mod-Pagespeed|X-Page-Speed'
