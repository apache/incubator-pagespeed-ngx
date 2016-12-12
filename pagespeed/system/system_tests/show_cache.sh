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
start_test ShowCache without URL gets a form, inputs, preloaded UA.
ADMIN_CACHE=$PRIMARY_SERVER/pagespeed_admin/cache
OUT=$($WGET_DUMP $ADMIN_CACHE)
check_from "$OUT" fgrep -q "<form>"
check_from "$OUT" fgrep -q "<input "
check_from "$OUT" fgrep -q "Cache-Control: max-age=0, no-cache"
# Preloaded user_agent value field leading with "Mozilla" set in
# ../automatic/system_test_helpers.sh to help test a "normal" flow.
check_from "$OUT" fgrep -q 'name=user_agent value="Mozilla'

start_test ShowCache with bogus URL gives a 404
OUT=$(check_error_code 8 $WGET -O - --save-headers \
  $PRIMARY_SERVER/pagespeed_cache?url=bogus_format 2>&1)
check_from "$OUT" fgrep -q "ERROR 404: Not Found"

start_test ShowCache with valid, present URL, with unique options.
options="PageSpeedImageInlineMaxBytes=6765"
fetch_until -save $EXAMPLE_ROOT/rewrite_images.html?$options \
    'grep -c Puzzle\.jpg\.pagespeed\.ic\.' 1
URL_TAIL=$(grep Puzzle $FETCH_UNTIL_OUTFILE | cut -d \" -f 2)
SHOW_CACHE_URL=$EXAMPLE_ROOT/$URL_TAIL
SHOW_CACHE_QUERY=$ADMIN_CACHE?url=$SHOW_CACHE_URL\&$options
echo "$WGET_DUMP $SHOW_CACHE_QUERY"
OUT=$($WGET_DUMP $SHOW_CACHE_QUERY)

# TODO(jmarantz): I have seen this test fail in the 'apache_debug_leak_test'
# phase of checkin tests.  The failure is that we are able to rewrite the
# HTML image link for Puzzle.jpg, but when fetching the metadata cache entry
# it comes back as cache_ok:false.
#
# This should be investigated, especially if it happens again.  Wild
# guess: the shared-memory metadata cache might face an unexpected
# eviction as it is not a pure LRU.
#
# Another guess: I think we might be calling callbacks with updated
# metadata cache before writing the metadata cache entries to the cache
# in some flows, e.g. in ResourceReconstructCallback::HandleDone.  It's not
# obvious why that flow would affect this test, but the ordering
# of writing cache and calling callbacks deserves an audit.
check_from "$OUT" fgrep -q cache_ok:true
check_from "$OUT" fgrep -q mod_pagespeed_example/images/Puzzle.jpg

function show_cache_after_flush() {
  start_test ShowCache with same URL and matching options misses after flush
  OUT=$($WGET_DUMP $SHOW_CACHE_QUERY)
  check_from "$OUT" fgrep -q cache_ok:false
}

on_cache_flush show_cache_after_flush

start_test ShowCache with same URL but new options misses.
options="PageSpeedImageInlineMaxBytes=6766"
OUT=$($WGET_DUMP $ADMIN_CACHE?url=$SHOW_CACHE_URL\&$options)
check_from "$OUT" fgrep -q cache_ok:false
