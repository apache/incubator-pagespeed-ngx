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
function php_ipro_record() {
  local url="$1"
  local max_cache_bytes="$2"
  local cache_bytes_op="$3"
  local cache_bytes_cmp="$4"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $url
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $url)
  check_from "$OUT" egrep -iq 'Content-Encoding: gzip'
  # Now check that we receive the optimized content from cache (not double
  # zipped). When the resource is optimized, "peachpuff" is rewritten to its
  # hex equivalent.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $url 'fgrep -c ffdab9' 1
  # The gzipped size is 175 with the default compression. Our compressed
  # cache uses gzip -9, which will compress even better (below 150).
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - \
    --header="Accept-Encoding: gzip" $url)
  # TODO(jcrowell): clean up when ngx_pagespeed stops chunk-encoding static
  # resources.
  bytes_from_cache=$(echo "$OUT" | wc -c)
  check [ $bytes_from_cache -lt $max_cache_bytes ]
  check [ $bytes_from_cache $cache_bytes_op $cache_bytes_cmp ]
  # Ensure that the Content-Encoding header matches the data that is sent.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    --header='Accept-Encoding: gzip' $url \
    | scrape_header 'Content-Encoding')
  check_from "$OUT" fgrep -q 'gzip'
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - $url)
  bytes_from_cache_uncompressed=$(echo "$OUT" | wc -c)
  # The data should uncompressed, but minified at this point.
  check [ $bytes_from_cache_uncompressed -gt 10000 ]
  check_from "$OUT" grep -q "ffdab9"
  # Ensure that the Content-Encoding header matches the data that is sent.
  # In this case we didn't sent the Accept-Encoding header, so we don't
  # expect the data to have any Content-Encoding header.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $url)
  check_not_from "$OUT" egrep -q 'Content-Encoding'
}
start_test Cache Compression On: PHP Pre-Gzipping does not get destroyed by the cache.
URL="http://compressedcache.example.com/mod_pagespeed_test/php_gzip.php"
# The gzipped size is 175 with the default compression. Our compressed
# cache uses gzip -9, which will compress even better (below 150).
php_ipro_record "$URL" 150 "-lt" 175
start_test Cache Compression Off: PHP Pre-Gzipping does not get destroyed by the cache.
# With cache compression off, we should see about 175 for both the pre and
# post optimized resource.
URL="http://uncompressedcache.example.com/mod_pagespeed_test/php_gzip.php"
php_ipro_record "$URL" 200 "-gt" 150
