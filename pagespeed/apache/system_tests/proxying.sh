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
start_test Issue 609 -- proxying non-.pagespeed content, and caching it locally
URL="$PRIMARY_SERVER/modpagespeed_http/not_really_a_font.woff"
echo $WGET_DUMP $URL ....
OUT1=$($WGET_DUMP $URL)
check_from "$OUT1" egrep -q "This is not really font data"
if [ $statistics_enabled = "1" ]; then
  OLDSTATS=$OUTDIR/proxy_fetch_stats.old
  NEWSTATS=$OUTDIR/proxy_fetch_stats.new
  $WGET_DUMP $STATISTICS_URL > $OLDSTATS
fi
OUT2=$($WGET_DUMP $URL)
check_from "$OUT2" egrep -q "This is not really font data"
if [ $statistics_enabled = "1" ]; then
  $WGET_DUMP $STATISTICS_URL > $NEWSTATS
  check_stat $OLDSTATS $NEWSTATS cache_hits 1
  check_stat $OLDSTATS $NEWSTATS cache_misses 0
fi

start_test Do not proxy content without a Content-Type header
# These tests depend on modpagespeed.com being configured to serve an example
# file with a content-type header on port 8091 and without one on port 8092.
# scripts/serve_proxying_tests.sh can do this.
URL="$PRIMARY_SERVER/content_type_absent/"
CONTENTS="This file should not be proxied"


OUT=$($CURL --include --silent $URL)
check_from "$OUT" fgrep -q "403 Forbidden"
check_from "$OUT" fgrep -q \
    "Missing Content-Type required for proxied resource"
check_not_from "$OUT" fgrep -q "$CONTENTS"

start_test But do proxy content if the Content-Type header is present.
URL="$PRIMARY_SERVER/content_type_present/"
CONTENTS="This file should be proxied"


OUT=$($CURL --include --silent $URL)
check_from "$OUT" fgrep -q "200 OK"
check_from "$OUT" fgrep -q "$CONTENTS"
