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
start_test PageSpeed resources should have a content length.
HTML_URL="$EXAMPLE_ROOT/rewrite_css_images.html?PageSpeedFilters=rewrite_css"
fetch_until -save "$HTML_URL" "fgrep -c rewrite_css_images.css.pagespeed.cf" 1
# Pull the rewritten resource name out so we get an accurate hash.
REWRITTEN_URL=$(grep rewrite_css_images.css $FETCH_UNTIL_OUTFILE | \
                awk -F'"' '{print $(NF-1)}')
if [[ $REWRITTEN_URL == *//* ]]; then
  URL="$REWRITTEN_URL"
else
  URL="$REWRITTEN_ROOT/$REWRITTEN_URL"
fi
# This will use REWRITE_DOMAIN as an http_proxy if set, otherwise no proxy.
OUT=$(http_proxy=${REWRITE_DOMAIN:-} $WGET_DUMP $URL)
check_from "$OUT" grep "^Content-Length:"
check_not_from "$OUT" grep "^Transfer-Encoding: chunked"
check_not_from "$OUT" grep "^Cache-Control:.*private"
