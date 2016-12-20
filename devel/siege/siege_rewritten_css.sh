#!/bin/sh
# Runs 'siege' on a single cache-extended URL cache-extended CSS file
# scraped from rewrite_css.html.
#
# Usage:
#   devel/siege/siege_extended_css.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# Fetch the rewrite_css example in cache-extend mode so we can get a small
# cache-extended CSS file.
EXAMPLE="http://localhost:8080/mod_pagespeed_example"

# The format of the 'link' HTML line we get is this:
# <link rel="stylesheet" type="text/css"
#     href="styles/yellow.css.pagespeed.ce.lzJ8VcVi1l.css">
# The line-break before 'href' is added here to avoid exceeding 80 cols
# in this script but is not in the HTML.
#
# Splitting this by quotes seems a little fragile but it gets us the
# URL in the 6th token.
extract_pagespeed_url $EXAMPLE/rewrite_css.html 'link rel=' 6 rewrite_css

run_siege "$EXAMPLE/$url"
