#!/bin/bash
#
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
# Test to make sure dynamically defined url-valued attributes are rewritten by
# rewrite_domains.  See mod_pagespeed_test/rewrite_domains.html: in addition
# to having one <img> URL, one <form> URL, and one <a> url it also has one
# <span src=...> URL, one <hr imgsrc=...> URL, one <hr src=...> URL, and one
# <blockquote cite=...> URL, all referencing src.example.com.  The first three
# should be rewritten because of hardcoded rules, the span.src and hr.imgsrc
# should be rewritten because of UrlValuedAttribute directives, the hr.src
# should be left unmodified, and the blockquote.src should be rewritten as an
# image because of a UrlValuedAttribute override.  The rewritten ones should
# all be rewritten to dst.example.com.
HOST_NAME="http://url-attribute.example.com"
TEST="$HOST_NAME/mod_pagespeed_test"
REWRITE_DOMAINS="$TEST/rewrite_domains.html"
UVA_EXTEND_CACHE="$TEST/url_valued_attribute_extend_cache.html"
UVA_EXTEND_CACHE+="?PageSpeedFilters=core,+left_trim_urls"

start_test Rewrite domains in dynamically defined url-valued attributes.

RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $REWRITE_DOMAINS)
MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
check [ $MATCHES -eq 6 ]
MATCHES=$(echo "$RESPONSE_OUT" | \
    fgrep -c '<hr src=http://src.example.com/hr-image>')
check [ $MATCHES -eq 1 ]

start_test Additional url-valued attributes are fully respected.

function count_exact_matches() {
  # Needed because "fgrep -c" counts lines with matches, not pure matches.
  fgrep -o "$1" | wc -l
}

# There are ten resources that should be optimized.
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $UVA_EXTEND_CACHE 'count_exact_matches .pagespeed.' 10

# Make sure <custom d=...> isn't modified at all, but that everything else is
# recognized as a url and rewritten from ../foo to /foo.  This means that only
# one reference to ../mod_pagespeed should remain, <custom d=...>.
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $UVA_EXTEND_CACHE 'grep -c d=.[.][.]/mod_pa' 1
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $UVA_EXTEND_CACHE 'fgrep -c ../mod_pa' 1

# There are ten images that should be optimized, so grep including .ic.
http_proxy=$SECONDARY_HOSTNAME \
    fetch_until $UVA_EXTEND_CACHE 'count_exact_matches .pagespeed.ic' 10

start_test url-valued stylesheet attributes are properly handled

function url_valued_attribute_css_optimization_status() {
  local input=$(cat)
  if [[ $(echo "$input" | fgrep -o ".pagespeed.cf." | wc -l) != 7 ]]; then
    echo incomplete  # still some unoptimized css files
  elif ! echo "$input" | \
           grep -q "<style>.bold{font-weight:bold}</style>"; then
    echo incomplete  # bold.css still not inlined
  else
    echo complete
  fi
}

URL="$TEST/url_valued_attribute_css.html"
URL+="?PageSpeedFilters=debug,combine_css,rewrite_css,inline_css"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
  url_valued_attribute_css_optimization_status complete
OUT=$(cat $FETCH_UNTIL_OUTFILE)
check_from "$OUT" grep \
  "Could not combine over barrier: custom or alternate stylesheet attribute"
check_from "$OUT" grep 'link data-stylesheet=[^<]*.pagespeed.cf'
LOOK_FOR="<span data-stylesheet-a=[^<]*.pagespeed.cf"
LOOK_FOR+="[^<]*data-stylesheet-b=[^<]*.pagespeed.cf"
LOOK_FOR+="[^<]*data-stylesheet-c=[^<]*.pagespeed.cf"
check_from "$OUT" grep "$LOOK_FOR"
check_from "$OUT" grep "<link rel=invalid data-stylesheet=[^<]*.pagespeed.cf"
check_from "$OUT" grep \
  "<style data-stylesheet=[^<]*.pagespeed.cf[^>]*>.bold{font-weight:bold}"
check_not_from "$OUT" fgrep "blue.css+yellow.css"
