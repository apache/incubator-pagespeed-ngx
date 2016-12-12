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
# Checks that inline_preview_images injects compiled javascript
test_filter inline_preview_images optimize mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
echo run_wget_with_args $URL
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 1
fetch_until $URL 'grep -c /\*' 0
check run_wget_with_args $URL

# Checks that inline_preview_images,debug injects from javascript
# in non-compiled mode
test_filter inline_preview_images,debug debug mode
FILE=delay_images.html?PageSpeedFilters=$FILTER_NAME
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
WGET_ARGS="${WGET_ARGS} --user-agent=iPhone"
fetch_until $URL 'grep -c pagespeed.delayImagesInit' 3
check run_wget_with_args $URL
