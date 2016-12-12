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
start_test OptimizeForBandwidth
# We use blocking-rewrite tests because we want to make sure we don't
# get rewritten URLs when we don't want them.
function test_optimize_for_bandwidth() {
  SECONDARY_HOST="optimizeforbandwidth.example.com"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME \
        $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest \
        $SECONDARY_HOST/mod_pagespeed_test/optimize_for_bandwidth/$1)
  check_from "$OUT" grep -q "$2"
  if [ "$#" -ge 3 ]; then
    check_from "$OUT" grep -q "$3"
  fi
}
test_optimize_for_bandwidth rewrite_css.html \
  '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
  '<link rel="stylesheet" type="text/css" href="yellow.css">'
test_optimize_for_bandwidth inline_css/rewrite_css.html \
  '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
  '<style>.yellow{background-color:#ff0}</style>'
test_optimize_for_bandwidth css_urls/rewrite_css.html \
  '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
  '<link rel="stylesheet" type="text/css" href="A.yellow.css.pagespeed'
test_optimize_for_bandwidth image_urls/rewrite_image.html \
  '<img src=\"xarrow.png.pagespeed.'
test_optimize_for_bandwidth core_filters/rewrite_css.html \
  '.blue{foreground-color:blue}body{background:url(xarrow.png.pagespeed.' \
  '<style>.yellow{background-color:#ff0}</style>'

# Make sure that optimize for bandwidth + CombineCSS doesn't eat
# URLs.
URL=http://optimizeforbandwidth.example.com/mod_pagespeed_example
URL=$URL/combine_css.html?PageSpeedFilters=+combine_css
OUT=$(http_proxy=$SECONDARY_HOSTNAME \
      $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest $URL)
check_from "$OUT" fgrep -q bold.css

# Same for CombineJS --- which never actually did, to best of my knowledge,
# but better check just in case.
URL=http://optimizeforbandwidth.example.com/mod_pagespeed_example
URL=$URL/combine_javascript.html?PageSpeedFilters=+combine_javascript
OUT=$(http_proxy=$SECONDARY_HOSTNAME \
      $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest $URL)
check_from "$OUT" fgrep -q combine_javascript2
