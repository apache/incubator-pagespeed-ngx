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
start_test Pass through headers when Cache-Control is set early on HTML.
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    http://issue809.example.com/mod_pagespeed_example/index.html \
    -O $TESTTMP/issue809.http
check_from "$(extract_headers $TESTTMP/issue809.http)" \
    grep -q "Issue809: Issue809Value"

start_test Pass through common headers from origin on combined resources.
URL="http://issue809.example.com/mod_pagespeed_example/combine_css.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    "grep -c css.pagespeed.cc." 1

# Extract out the rewritten CSS URL from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file and look inside for the sprited image reference (ic.pagespeed.is...).
CSS=$(grep stylesheet "$FETCH_UNTIL_OUTFILE" | cut -d\" -f 6)
if [ ${CSS:0:7} != "http://" ]; then
  CSS="http://issue809.example.com/mod_pagespeed_example/$CSS"
fi
http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $CSS -O $TESTTMP/combined.http
check_from "$(extract_headers $TESTTMP/combined.http)" \
    grep -q "Issue809: Issue809Value"
