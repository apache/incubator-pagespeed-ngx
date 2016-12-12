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
start_test query params dont turn on core filters
# See https://github.com/pagespeed/ngx_pagespeed/issues/1190
URL="debug-filters.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.html?PageSpeedFilters=-rewrite_css"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
FILTERS=$(extract_filters_from_debug_html "$OUT")
check_from "$FILTERS" grep -q "^db.*Debug$"
check_from "$FILTERS" grep -q "^hw.*Flushes html$"
check_not_from "$FILTERS" grep -q "^jm.*Rewrite External Javascript$"
check_not_from "$FILTERS" grep -q "^jj.*Rewrite Inline Javascript$"
