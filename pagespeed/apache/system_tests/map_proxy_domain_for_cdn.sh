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
start_test MapProxyDomain for CDN setup
# Test transitive ProxyMapDomain.  In this mode we have three hosts: cdn,
# proxy, and origin.  Proxy runs MPS and fetches resources from origin,
# optimizes them, and rewrites them to CDN for serving. The CDN is dumb and
# has no caching so simply proxies all requests to proxy.  Origin serves out
# images only.
echo "Rewrite HTML with reference to proxyable image on CDN."
PROXY_PM="http://proxy.pm.example.com"
URL="$PROXY_PM/transitive_proxy.html"
PDT_STATSDIR=$TESTTMP/stats
rm -rf $PDT_STATSDIR
mkdir -p $PDT_STATSDIR
PDT_OLDSTATS=$PDT_STATSDIR/blocking_rewrite_stats.old
PDT_NEWSTATS=$PDT_STATSDIR/blocking_rewrite_stats.new
PDT_PROXY_STATS_URL=$PROXY_PM/mod_pagespeed_statistics?PageSpeed=off
http_proxy=$SECONDARY_HOSTNAME \
  $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_OLDSTATS

# The image should be proxied from origin, compressed, and rewritten to cdn.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save -recursive $URL \
  'fgrep -c cdn.pm.example.com/external/xPuzzle.jpg.pagespeed.ic' 1
check_file_size "$WGET_DIR/xPuzzle*" -lt 241260

# Make sure that the file was only rewritten once.
http_proxy=$SECONDARY_HOSTNAME \
  $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_NEWSTATS
check_stat $PDT_OLDSTATS $PDT_NEWSTATS image_rewrites 1

# The js should be fetched locally and inlined.
http_proxy=$SECONDARY_HOSTNAME \
  fetch_until -save -recursive $URL 'fgrep -c document.write' 1

# Save the image URL so we can try to reconstruct it later.
PDT_IMG_URL=`egrep -o \"[^\"]*xPuzzle[^\"]*\.pagespeed[^\"]*\" $FETCH_FILE | \
  sed -e 's/\"//g'`

# This function will be called after the cache is flushed to test
# reconstruction.
function map_proxy_domain_cdn_reconstruct() {
  rm -rf $PDT_STATSDIR
  mkdir -p $PDT_STATSDIR
  http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_OLDSTATS
  echo "Make sure we can reconstruct the image."
  # Fetch the url until it is less than its original size (i.e. compressed).
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $PDT_IMG_URL "wc -c" 241260 "" "-lt"
  # Double check that we actually reconstructed.
  http_proxy=$SECONDARY_HOSTNAME \
    $WGET_DUMP $PDT_PROXY_STATS_URL > $PDT_NEWSTATS
  check_stat $PDT_OLDSTATS $PDT_NEWSTATS image_rewrites 1
}
on_cache_flush map_proxy_domain_cdn_reconstruct
