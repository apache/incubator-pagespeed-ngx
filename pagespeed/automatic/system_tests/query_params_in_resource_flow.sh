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
start_test Query params and headers are recognized in resource flow.
URL=$REWRITTEN_ROOT/styles/W.rewrite_css_images.css.pagespeed.cf.Hash.css
echo "Image gets rewritten by default."
# TODO(sligocki): Replace this fetch_until with single blocking fetch once
# the blocking rewrite header below works correctly.
WGET_ARGS="--header='X-PSA-Blocking-Rewrite:psatest'"
fetch_until $URL 'fgrep -c BikeCrashIcn.png.pagespeed.ic' 1
echo "Image doesn't get rewritten when we turn it off with headers."
# The space after '-convert_png_to_jpeg,' is to test that we do strip spaces.
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  --header="PageSpeedFilters:-convert_png_to_jpeg, -recompress_png" $URL)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"

# TODO(vchudnov): This test is not doing quite what it advertises. It
# seems to be getting the cached rewritten resource from the previous
# test case and not going into image.cc itself. Removing the previous
# test case causes this one to go into image.cc. We should test with a
# different resource.
echo "Image doesn't get rewritten when we turn it off with query params."
OUT=$($WGET_DUMP --header="X-PSA-Blocking-Rewrite:psatest" \
  $URL?PageSpeedFilters=-convert_png_to_jpeg,-recompress_png)
check_not_from "$OUT" fgrep -q "BikeCrashIcn.png.pagespeed.ic"
