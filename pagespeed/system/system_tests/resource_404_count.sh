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
start_test 404s are served and properly recorded.
echo $STATISTICS_URL
NUM_404=$(scrape_stat resource_404_count)
echo "Initial 404s: $NUM_404"
WGET_ERROR=$(check_not $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
check_from "$WGET_ERROR" fgrep -q "404 Not Found"

# Check that the stat got bumped.
NUM_404_FINAL=$(scrape_stat resource_404_count)
echo "Final 404s: $NUM_404_FINAL"
check [ $(expr $NUM_404_FINAL - $NUM_404) -eq 1 ]

# Check that the stat doesn't get bumped on non-404s.
URL="$PRIMARY_SERVER/mod_pagespeed_example/styles/"
URL+="W.rewrite_css_images.css.pagespeed.cf.Hash.css"
OUT=$(wget -O - -q $URL)
check_from "$OUT" grep background-image
NUM_404_REALLY_FINAL=$(scrape_stat resource_404_count)
check [ $NUM_404_FINAL -eq $NUM_404_REALLY_FINAL ]
