#!/bin/sh

# Tests that mod_pagespeed can fetch HTTPS resources.  Note that mod_pagespeed
# does not work like this by default: it must be recompiled to incorporate the
# openssl library and to add calls to that library to handle HTTPS resources.

echo Testing that HTTPS fetching is enabled and working in mod_pagespeed.
echo Note that this test will fail with timeouts if the serf fetcher has not
echo been compiled in.

this_dir="$( dirname "${BASH_SOURCE[0]}" )"
source "$this_dir/system_test_helpers.sh" || exit 1

echo Test that we can rewrite a proxied resource fetched with HTML
fetch_until $TEST_ROOT/https_fetch/https_fetch.html 'grep -c /modpagespeed_dot_com/xlogo' 1

echo Test that we can rewrite an HTTPS resource without changing the domain.
fetch_until $TEST_ROOT/https_fetch/https_fetch.html 'grep -c https://modpagespeed.com/mod_pagespeed_example/images/xPuzzle' 1

