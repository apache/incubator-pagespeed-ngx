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
start_test resize images and modify URLs
rm -rf $OUTDIR
mkdir $OUTDIR
HTML_PATH="$TEST_ROOT/webp_rewriting/"
HTML_OPT="?PageSpeedFilters=rewrite_images,rewrite_css,convert_to_webp_animated"
HTML_OPT="$HTML_OPT&$IMAGES_QUALITY=75&$WEBP_QUALITY=65"
function resize_image_test {
  local url=$1
  local expected_num=$2
  local url_opt="$HTML_PATH$url$HTML_OPT"
  fetch_until -save -recursive $url_opt \
    'fgrep -c .pagespeed.ic.' $expected_num \
    --user-agent=webp-animated
}
# Check that the images will be resized.
HTML_URL="resizable_images.html"
HTML_FETCHED="$WGET_DIR/$HTML_URL$HTML_OPT"
resize_image_test $HTML_URL 4
#
IMAGE="/120x150xPuzzle2.jpg.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -le 2340
#
IMAGE="/90xNxIronChef2.gif.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -le 1624
#
IMAGE="/Nx50xBikeCrashIcn.png.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -le 822
#
IMAGE="/120x120xpagespeed_logo.png.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -le 9214
#
# Check that the images will be recompressed without resizing.
HTML_URL="unresizable_images.html"
HTML_FETCHED="$WGET_DIR/$HTML_URL$HTML_OPT"
resize_image_test $HTML_URL 3
#
IMAGE="/xdisclosure_open_plus.png.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -ge 150
check_file_size "$WGET_DIR$IMAGE" -lt 299
#
IMAGE="/xgray_saved_as_rgb.webp.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -ge 270
check_file_size "$WGET_DIR$IMAGE" -lt 318
#
IMAGE="/xPuzzle2.jpg.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -ge 12212
check_file_size "$WGET_DIR$IMAGE" -lt 31066
#
# Check that animated images will be recompressed without resizing.
HTML_URL="animated_images.html"
HTML_FETCHED="$WGET_DIR/$HTML_URL$HTML_OPT"
echo Debug start: $(date +"%T")
resize_image_test $HTML_URL 1
echo Debug end: $(date +"%T")
#
IMAGE="/xPageSpeedAnimationSmall.gif.pagespeed.ic.*.webp"
check_from "$(< $HTML_FETCHED)" grep -q $IMAGE
check_file_size "$WGET_DIR$IMAGE" -ge 7232
check_file_size "$WGET_DIR$IMAGE" -lt 26251
