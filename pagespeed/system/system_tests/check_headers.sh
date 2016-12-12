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
start_test Check for correct default pagespeed header format.
# This will be X-Page-Speed in nginx and X-ModPagespeed in apache.  Accept both.
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q \
  '^X-(Mod-Pagespeed|Page-Speed): [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+-[0-9]+'

start_test pagespeed is defaulting to more than PassThrough
if [ ! -z "${APACHE_DOC_ROOT-}" ]; then
  # Note: in Apache this test relies on lack of .htaccess in mod_pagespeed_test.
  check [ ! -f $APACHE_DOC_ROOT/mod_pagespeed_test/.htaccess ]
fi
fetch_until $TEST_ROOT/bot_test.html 'fgrep -c .pagespeed.' 2

start_test ipro resources have etag and not last-modified
URL="$EXAMPLE_ROOT/images/Puzzle.jpg?a=$RANDOM"
# Fetch it a few times until IPRO is done and has given it an ipro ("aj") etag.
fetch_until -save "$URL" 'grep -c E[Tt]ag:.W/.PSA-aj.' 1 --save-headers
# Verify that it doesn't have a Last-Modified header.
check [ $(grep -c "^Last-Modified:" $FETCH_FILE) = 0 ]
# Extract the Etag and verify we get "not modified" when we send the it.
ETAG=$(grep -a -o 'W/"PSA-aj[^"]*"' $FETCH_FILE)
check_from "$ETAG" fgrep PSA
echo $CURL -sS -D- -o/dev/null -H "If-None-Match: $ETAG" $URL
OUT=$($CURL -sS -D- -o/dev/null -H "If-None-Match: $ETAG" $URL)
check_from "$OUT" fgrep "HTTP/1.1 304"
check_not_from "$OUT" fgrep "Content-Length"
# Verify we don't get a 304 with a different Etag.
BAD_ETAG=$(echo "$ETAG" | sed s/PSA-aj/PSA-ic/)
echo $CURL -sS -D- -o/dev/null -H "If-None-Match: $BAD_ETAG" $URL
OUT=$($CURL -sS -D- -o/dev/null -H "If-None-Match: $BAD_ETAG" $URL)
check_not_from "$OUT" fgrep "HTTP/1.1 304"
check_from "$OUT" fgrep "HTTP/1.1 200"
check_from "$OUT" fgrep "Content-Length"
