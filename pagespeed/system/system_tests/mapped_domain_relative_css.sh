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
# http://github.com/pagespeed/mod_pagespeed/issues/494 -- test
# that fetching a css with embedded relative images from a different
# VirtualHost, accessing the same content, and rewrite-mapped to the
# primary domain, delivers results that are cached for a year, which
# implies the hash matches when serving vs when rewriting from HTML.
#
# This rewrites the CSS, absolutifying the embedded relative image URL
# reference based on the the main server host.
start_test Relative images embedded in a CSS file served from a mapped domain
DIR="mod_pagespeed_test/map_css_embedded"
URL="http://www.example.com/$DIR/issue494.html"
MAPPED_PREFIX="$DIR/A.styles.css.pagespeed.cf"
http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
    "grep -c cdn.example.com/$MAPPED_PREFIX" 1
MAPPED_CSS=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL | \
    grep -o "$MAPPED_PREFIX..*.css")

# Now fetch the resource using a different host, which is mapped to the first
# one.  To get the correct bytes, matching hash, and long TTL, we need to do
# apply the domain mapping in the CSS resource fetch.
URL="http://origin.example.com/$MAPPED_CSS"
echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL
CSS_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
check_from "$CSS_OUT" fgrep -q "Cache-Control: max-age=31536000"
