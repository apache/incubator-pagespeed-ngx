#!/bin/bash

# Tests that mod_pagespeed can fetch HTTPS resources.  Note that mod_pagespeed
# does not work like this by default: a flag must be specified in
# pagespeed.conf:
#   ModPagespeedFetchHttps enable

echo Testing that HTTPS fetching is enabled and working in mod_pagespeed.
echo Note that this test will fail with timeouts if the serf fetcher has not
echo been compiled in.

this_dir="$( dirname "${BASH_SOURCE[0]}" )"
INSTAWEB_CODE_DIR="$this_dir/../net/instaweb"
if [ ! -e "$INSTAWEB_CODE_DIR" ] ; then
  INSTAWEB_CODE_DIR="$this_dir/../../"
fi
source "$INSTAWEB_CODE_DIR/automatic/system_test_helpers.sh" || exit 1

echo Test that we can rewrite an HTTPS resource from a domain with a valid cert.
fetch_until $TEST_ROOT/https_fetch/https_fetch.html \
  'grep -c /https_gstatic_dot_com/1.gif.pagespeed.ce' 1
