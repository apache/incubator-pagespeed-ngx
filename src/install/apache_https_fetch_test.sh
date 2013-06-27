#!/bin/bash

# Tests that mod_pagespeed can fetch HTTPS resources.  Note that mod_pagespeed
# does not work like this by default: a flag must be specified in
# pagespeed.conf:
#   ModPagespeedFetchHttps enable

echo Testing that HTTPS fetching is enabled and working in mod_pagespeed.
echo Note that this test will fail with timeouts if the serf fetcher has not
echo been compiled in.

this_dir="$(dirname $(readlink -f $0))"
source "$this_dir/system_test_helpers.sh" || exit 1

echo Test that we can rewrite an HTTPS resource from a domain with a valid cert.
fetch_until $TEST_ROOT/https_fetch/https_fetch.html \
  'grep -c /https_gstatic_dot_com/devconsole/xpss-architecture.png.pagespeed.ic' 1
