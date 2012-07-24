#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework (furious) tests.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Argument 1 should be the host:port of the Apache server to talk to.
#
this_dir=$(dirname $0)
source "$this_dir/system_test_helpers.sh" || exit 1

TEST="$1/mod_pagespeed_test"
REWRITE_DOMAINS="$TEST/rewrite_domains.html"
UVA_EXTEND_CACHE="$TEST/url_valued_attribute_extend_cache.html"

echo TEST: Rewrite domains in dynamically defined url-valued attributes.
check [ 5 = $($WGET_DUMP $REWRITE_DOMAINS | fgrep -c http://dst.example.com) ]
check [ 1 = $($WGET_DUMP $REWRITE_DOMAINS |
  fgrep -c '<hr src=http://src.example.com/hr-image>') ]

echo TEST: Additional url-valued attributes are fully respected.

# There are seven resources that should be optimized
fetch_until $UVA_EXTEND_CACHE 'fgrep -c .pagespeed.' 7

# Make sure <custom d=...> isn't modified at all, but that everything else is
# recognized as a url and rewritten from ../foo to /foo.  This means that only
# one reference to ../mod_pagespeed should remain, <custom d=...>.
check [ 1 = $($WGET_DUMP $UVA_EXTEND_CACHE | fgrep -c ' d="../mod_pa') ]
check [ 1 = $($WGET_DUMP $UVA_EXTEND_CACHE | fgrep -c '../mod_pa') ]

# There are five images that should be optimized.
check [ 5 = $($WGET_DUMP $UVA_EXTEND_CACHE | fgrep -c '.pagespeed.ic') ]


echo "PASS."
