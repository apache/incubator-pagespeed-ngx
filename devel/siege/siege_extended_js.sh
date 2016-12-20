#!/bin/sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# Fetch the rewrite_css example in cache-extend mode so we can get a small
# cache-extended CSS file.
EXAMPLE="http://localhost:8080/mod_pagespeed_example"

# The format of the 'script' HTML line we want is this:
# <script src="rewrite_javascript.js" ...
extract_pagespeed_url $EXAMPLE/rewrite_javascript.html 'script src=' \
  2 extend_cache

run_siege "$EXAMPLE/$url"
