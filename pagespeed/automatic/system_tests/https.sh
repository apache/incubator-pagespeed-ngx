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
start_test Simple test that https is working.
if [ -n "$HTTPS_HOST" ]; then
  URL="$HTTPS_EXAMPLE_ROOT/combine_css.html"
  fetch_until $URL 'fgrep -c css+' 1 --no-check-certificate

  start_test https is working.
  echo $WGET_DUMP_HTTPS $URL
  HTML_HEADERS=$($WGET_DUMP_HTTPS $URL)

  echo Checking for X-Mod-Pagespeed header
  check_from "$HTML_HEADERS" egrep -q 'X-Mod-Pagespeed|X-Page-Speed'

  echo Checking for combined CSS URL
  EXPECTED='href="styles/yellow\.css+blue\.css+big\.css+bold\.css'
  EXPECTED="$EXPECTED"'\.pagespeed\.cc\..*\.css"/>'
  fetch_until "$URL?PageSpeedFilters=combine_css,trim_urls" \
      "grep -ic $EXPECTED" 1 --no-check-certificate

  echo Checking for combined CSS URL without URL trimming
  # Without URL trimming we still preserve URL relativity.
  fetch_until "$URL?PageSpeedFilters=combine_css" "grep -ic $EXPECTED" 1 \
     --no-check-certificate
fi
