#!/bin/sh
# Runs 'siege' on a HTML file, but with 400k unique query-params.  We
# use 400k because a typical siege covers >300k transactions and we
# want to avoid repeats.
#
# Usage:
#   devel/siege/siege_html_high_entropy.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# Generate a list of unique URLs, each of which resolving to the same trival
# HTML file.  I don't see an easy way of specifying zero rewriters (default is
# CoreFilters) but by specifying a single rewriter "rewrite_domains" as a
# query-param, we can emulate that.  Note that "rewrite_domains" doesn't do
# anything if there are no domain-mappings set up.
#
# TODO(jmarantz): There appears to be no better way to turn all
# filters off via query-param.  Though you might think that
# PageSpeedRewriteLevel=PassThrough should work, it does not.  There
# is special handling for PageSpeedFilters=core but not for
# PassThrough.
echo "Generating URLs..."
urls="/tmp/high_entropy_urls.list.$$"
> "$urls"
trap "rm -f $urls" EXIT
base_url="http://localhost:8080/mod_pagespeed_example/collapse_whitespace.html?PageSpeedFilters=rewrite_domains&q"
for i in {1..400000}; do
  echo "$base_url=$i" >> "$urls"
done

run_siege --file="$urls"
