start_test static asset urls are mapped/sharded

HOST_NAME="http://map-static-domain.example.com"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    $HOST_NAME/mod_pagespeed_example/rewrite_javascript.html)
check_from "$OUT" fgrep \
  "http://static-cdn.example.com/$PSA_JS_LIBRARY_URL_PREFIX/js_defer"
