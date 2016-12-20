#!/bin/sh
# Runs 'siege' on a HTML file.
#
# Usage:
#   devel/siege/siege_html.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# TODO(jmarantz): There appears to be no better way to turn all
# filters off via query-param.  Though you might think that
# PageSpeedRewriteLevel=PassThrough should work, it does not.  There
# is special handling for PageSpeedFilters=core but not for
# PassThrough.
URL="http://localhost:8080/mod_pagespeed_example/collapse_whitespace.html?PageSpeedFilters=rewrite_domains"
run_siege "$URL"
