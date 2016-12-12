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
# Test max_cacheable_response_content_length.  There are two Javascript files
# in the html file.  The smaller Javascript file should be rewritten while
# the larger one shouldn't.
start_test Maximum length of cacheable response content.
HOST_NAME="http://max-cacheable-content-length.example.com"
DIR_NAME="mod_pagespeed_test/max_cacheable_content_length"
HTML_NAME="test_max_cacheable_content_length.html"
URL=$HOST_NAME/$DIR_NAME/$HTML_NAME
RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header \
    'X-PSA-Blocking-Rewrite: psatest' $URL)
check_from     "$RESPONSE_OUT" fgrep -qi small.js.pagespeed.
check_not_from "$RESPONSE_OUT" fgrep -qi large.js.pagespeed.

start_test LoadFromFile with length limits
# lff-large-files* have length limits set so that bold.css can be loaded from
# file, but big.css cannot be.

BASE="http://lff-large-files.example.com/mod_pagespeed_example/styles"
URL="$BASE/bold.css"
# Loads fine, as expected, because it's small.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

URL="$BASE/big.css"
# Still loads fine, because after we blocked LFF we DECLINED the request and
# loaded it through the non-LFF path.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

HOST="http://lff-large-files-no-fallback.example.com"
URL="$HOST/bold.css"
# Loads fine, as expected, because it's small.
http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

URL="$HOST/big.css"
# Doesn't load at all, because it's too big and we've configured a LFF path
# that's different from the one you'd get without LFF.
http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL
