# Test that we work fine with an explicitly configured SHM metadata cache.
start_test Using SHM metadata cache
HOST_NAME="http://shmcache.example.com"
URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until $URL 'grep -c .pagespeed.ic' 2
