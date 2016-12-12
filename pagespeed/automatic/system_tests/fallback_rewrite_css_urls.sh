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
start_test fallback_rewrite_css_urls works.
FILE=fallback_rewrite_css_urls.html?\
PageSpeedFilters=fallback_rewrite_css_urls,rewrite_css,extend_cache
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c Cuppa.png.pagespeed.ce.' 1  # image cache extended
fetch_until -save $URL 'grep -c fallback_rewrite_css_urls.css.pagespeed.cf.' 1
# Test this was fallback flow -> no minification.
check grep -q "body { background" $FETCH_FILE
