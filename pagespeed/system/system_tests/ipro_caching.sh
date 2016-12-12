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
start_test IPRO flow uses cache as expected.
# TODO(sligocki): Use separate VHost instead to separate stats.
STATS=$OUTDIR/blocking_rewrite_stats
IPRO_HOST=http://ipro.example.com
IPRO_ROOT=$IPRO_HOST/mod_pagespeed_test/ipro
URL=$IPRO_ROOT/test_image_dont_reuse2.png
IPRO_STATS_URL=$IPRO_HOST/pagespeed_admin/statistics
OUTFILE=$OUTDIR/ipro_output

# Initial stats.
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.0

# First IPRO request.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.1

# Resource not in cache the first time.
check_stat $STATS.0 $STATS.1 cache_hits 0
check_stat $STATS.0 $STATS.1 cache_misses 1
check_stat $STATS.0 $STATS.1 ipro_served 0
check_stat $STATS.0 $STATS.1 ipro_not_rewritable 0
# So we run the ipro recorder flow and insert it into the cache.
check_stat $STATS.0 $STATS.1 ipro_not_in_cache 1
check_stat $STATS.0 $STATS.1 ipro_recorder_resources 1
check_stat $STATS.0 $STATS.1 ipro_recorder_inserted_into_cache 1
# Image doesn't get rewritten the first time.
# TODO(sligocki): This should change to 1 when we get image rewrites started
# in the Apache output filter flow.
check_stat $STATS.0 $STATS.1 image_rewrites 0

# Second IPRO request.
# Original file has content-length 15131.  Once ipro-optimized, it is
# 11395, so fetch it until it's less than 12000.
http_proxy=$SECONDARY_HOSTNAME fetch_until $URL "wc -c" 12000 "" "-lt"
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.2

# Resource is found in cache the second time.
check_stat_op $STATS.1 $STATS.2 cache_hits 1 -ge
check_stat_op $STATS.1 $STATS.2 ipro_served 1 -ge
check_stat $STATS.1 $STATS.2 ipro_not_rewritable 0
# So we don't run the ipro recorder flow.
check_stat $STATS.1 $STATS.2 ipro_not_in_cache 0
check_stat $STATS.1 $STATS.2 ipro_recorder_resources 0
# Image gets rewritten on the second pass through this filter.
# TODO(sligocki): This should change to 0 when we get image rewrites started
# in the Apache output filter flow.
#
# Note also that image_rewrite stats are inherently flaky because locks are
# advisory, and steals may occur in valgrind, so we check for image rewrites
# being in the range 1:2.
check_stat_op $STATS.1 $STATS.2 image_rewrites 1 -ge
check_stat_op $STATS.1 $STATS.2 image_rewrites 2 -le

http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O $OUTFILE
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.3

check_stat $STATS.2 $STATS.3 cache_hits 1
check_stat $STATS.2 $STATS.3 ipro_served 1
check_stat $STATS.2 $STATS.3 ipro_recorder_resources 0
# Allow some slop in image_rewrites stat due to, I suspect, advisory locks.
check_stat_op $STATS.2 $STATS.3 image_rewrites 1 -le

# Check that the IPRO served file didn't discard any Apache err_response_out
# headers.  Only do this on servers that support err_response_out, so first
# check that X-TestHeader is ever set.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_ROOT/)
check_from "$OUT" fgrep "Content-Type: text/html"
if echo "$OUT" | grep -q "^X-TestHeader:"; then
  check_from "$(extract_headers $OUTFILE)" grep -q "X-TestHeader: hello"
fi

start_test "IPRO flow doesn't copy uncacheable resources multiple times."
URL=$IPRO_ROOT/nocache/test_image_dont_reuse.png

# Initial stats.
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.0

# First IPRO request.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.1

# Resource not in cache the first time.
check_stat $STATS.0 $STATS.1 cache_hits 0
check_stat $STATS.0 $STATS.1 cache_misses 1
check_stat $STATS.0 $STATS.1 ipro_served 0
check_stat $STATS.0 $STATS.1 ipro_not_rewritable 0
# So we run the ipro recorder flow, but the resource is not cacheable.
check_stat $STATS.0 $STATS.1 ipro_not_in_cache 1
check_stat $STATS.0 $STATS.1 ipro_recorder_resources 1
check_stat $STATS.0 $STATS.1 ipro_recorder_not_cacheable 1
# Uncacheable, so no rewrites.
check_stat $STATS.0 $STATS.1 image_rewrites 0
check_stat $STATS.0 $STATS.1 image_ongoing_rewrites 0

# Second IPRO request.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.2

check_stat $STATS.1 $STATS.2 cache_hits 0
# Note: This should load a RecentFetchFailed record from cache, but that
# is reported as a cache miss.
check_stat $STATS.1 $STATS.2 cache_misses 1
check_stat $STATS.1 $STATS.2 ipro_served 0
check_stat $STATS.1 $STATS.2 ipro_not_rewritable 1
# Important: We do not record this resource the second and third time
# because we remember that it was not cacheable.
check_stat $STATS.1 $STATS.2 ipro_not_in_cache 0
check_stat $STATS.1 $STATS.2 ipro_recorder_resources 0
check_stat $STATS.1 $STATS.2 image_rewrites 0
check_stat $STATS.1 $STATS.2 image_ongoing_rewrites 0

http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.3

# Same as second fetch.
check_stat $STATS.2 $STATS.3 cache_hits 0
check_stat $STATS.2 $STATS.3 cache_misses 1
check_stat $STATS.2 $STATS.3 ipro_not_rewritable 1
check_stat $STATS.2 $STATS.3 ipro_recorder_resources 0
check_stat $STATS.2 $STATS.3 image_rewrites 0
check_stat $STATS.2 $STATS.3 image_ongoing_rewrites 0

# Check that IPRO served resources that don't specify a cache control
# value are given the TTL specified by the ImplicitCacheTtlMs directive.
start_test "IPRO respects ImplicitCacheTtlMs."
HTML_URL=$IPRO_ROOT/no-cache-control-header/ipro.html
RESOURCE_URL=$IPRO_ROOT/no-cache-control-header/test_image_dont_reuse.png
RESOURCE_HEADERS=$OUTDIR/resource_headers
OUTFILE=$OUTDIR/ipro_resource_output

# Fetch the HTML to initiate rewriting and caching of the image.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $HTML_URL -O $OUTFILE

# First IPRO resource request after a short wait: it will never be optimized
# because our non-load-from-file flow doesn't support that, but it will have
# the full TTL.
sleep 2
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $RESOURCE_URL -O $OUTFILE
check_file_size "$OUTFILE" -gt 15000 # not optimized
RESOURCE_MAX_AGE=$( \
  extract_headers $OUTFILE | \
  grep 'Cache-Control:' | tr -d '\r' | \
  sed -e 's/^ *Cache-Control: *//' | sed -e 's/^.*max-age=\([0-9]*\).*$/\1/')
check test -n "$RESOURCE_MAX_AGE"
check test $RESOURCE_MAX_AGE -eq 333

# Second IPRO resource request after a short wait: it will still be optimized
# and the TTL will be reduced.
sleep 2
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $RESOURCE_URL -O $OUTFILE
check_file_size "$OUTFILE" -lt 15000 # optimized
RESOURCE_MAX_AGE=$( \
  extract_headers $OUTFILE | \
  grep 'Cache-Control:' | tr -d '\r' | \
  sed -e 's/^ *Cache-Control: *//' | sed -e 's/^.*max-age=\([0-9]*\).*$/\1/')
check test -n "$RESOURCE_MAX_AGE"

check test $RESOURCE_MAX_AGE -lt 333
check test $RESOURCE_MAX_AGE -gt 300
