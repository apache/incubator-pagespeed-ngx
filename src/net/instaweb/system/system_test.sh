#!/bin/bash
#
# Runs system tests for system/ and automatic/.
#
# See automatic/system_test_helpers.sh for usage.
#

# Run the automatic/ system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test.sh" || exit 1

# TODO(jefftk): move all tests from apache/system_test.sh to here except the
# ones that actually are Apache-specific.

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test load from file with ipro
  URL="http://lff-ipro.example.com/mod_pagespeed_test/lff_ipro/fake.woff"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - $URL)
  check_from "$OUT" grep "^This isn't really a woff file\.$"
  check [ "$(echo "$OUT" | wc -l)" = 1 ]

  start_test max cacheable content length with ipro
  URL="http://max-cacheable-content-length.example.com/mod_pagespeed_example/"
  URL+="images/BikeCrashIcn.png"
  # This used to check-fail the server; see ngx_pagespeed issue #771.
  http_proxy=$SECONDARY_HOSTNAME check $WGET -t 1 -O /dev/null $URL
fi

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  # Tests for individual URL purging, and for global cache purging via
  # GET pagespeed_admin/cache?purge=URL, and PURGE URL methods.
  PURGE_ROOT="http://purge.example.com"
  PURGE_STATS_URL="$PURGE_ROOT/pagespeed_admin/statistics"
  function cache_purge() {
    local purge_method="$1"
    local purge_path="$2"
    if [ "$purge_method" = "GET" ]; then
      echo http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
      http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
    else
      PURGE_URL="$PURGE_ROOT/$purge_path"
      echo $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
      check $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
    fi
    if [ $statistics_enabled -eq "0" ]; then
      # Without statistics, we have no mechanism to transmit state-changes
      # from one Apache child process to another, and so each process must
      # independently poll the cache.purge file, which happens every 5 seconds.
      echo sleep 6
      sleep 6
    fi
  }

  # Checks to see whether a .pagespeed URL is present in the metadata cache.
  # A response including "cache_ok:true" or "cache_ok:false" is send to stdout.
  function read_metadata_cache() {
    path="$PURGE_ROOT/mod_pagespeed_example/$1"
    http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?url=$path"
  }

  # Find the full .pagespeed. URL of yellow.css
  PURGE_COMBINE_CSS="$PURGE_ROOT/mod_pagespeed_example/combine_css.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
  yellow_css=$(grep yellow.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)
  blue_css=$(grep blue.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)

  for method in $CACHE_PURGE_METHODS; do
    start_test Individual URL Cache Purging with $method
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    cache_purge $method "*"
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:false

    sleep 1
    STATS=$OUTDIR/purge.stats
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.0
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.1

    # Having rewritten 4 CSS files, we will have done 4 resources fetches.
    check_stat $STATS.0 $STATS.1 num_resource_fetch_successes 4

    # Sanity check: rewriting the same CSS file results in no new fetches.
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.2
    check_stat $STATS.1 $STATS.2 num_resource_fetch_successes 0

    # Now flush one of the files, and it should be the only one that
    # needs to be refetched after we get the combine_css file again.
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    cache_purge $method mod_pagespeed_example/styles/yellow.css
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true

    sleep 1
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.3
    check_stat $STATS.2 $STATS.3 num_resource_fetch_successes 1
  done
fi
