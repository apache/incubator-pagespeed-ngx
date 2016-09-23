#!/bin/sh
#
# Deletes the specified cache directories and the apache log.  Note that this
# is not necessary or sufficient for purging the cache in a running system,
# because it does not affect the in-memory cache, or any external caches like
# memcached or redis.  This is used for ensuring a clean slate at the start
# of tests.
#
# This reads environment variables APACHE_LOG and MOD_PAGESPEED_CACHE, which
# are exported by pagespeed/install/Makefile.tests.

set -u
set -e

(set -x; rm -f "$APACHE_LOG")

dir_count=0
for dir in "$MOD_PAGESPEED_CACHE"*; do
  dir_count=$((dir_count + 1))
done

i=0
for dir in "$MOD_PAGESPEED_CACHE"*; do
  i=$((i + 1))
  echo rm -rf "$dir" "($i/$dir_count)"
  rm -rf "$dir"
done
