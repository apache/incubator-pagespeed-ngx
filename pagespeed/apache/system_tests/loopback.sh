# Test that loopback route fetcher works with vhosts not listening on
# 127.0.0.1  Only run this during CACHE_FLUSH_TEST as that is when
# APACHE_TERTIARY_PORT is set.
if [ "${APACHE_TERTIARY_PORT:-}" != "" ]; then
  start_test IP choice for loopback fetches.
  HOST_NAME="loopbackfetch.example.com"
  URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
  http_proxy=127.0.0.2:$APACHE_TERTIARY_PORT \
    fetch_until $URL 'grep -c .pagespeed.ic' 2
fi
