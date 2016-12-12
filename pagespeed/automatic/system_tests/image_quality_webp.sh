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
start_test quality of webp output images
rm -rf $OUTDIR
mkdir $OUTDIR
IMG_REWRITE="$TEST_ROOT/webp_rewriting/rewrite_images.html"
REWRITE_URL="$IMG_REWRITE?PageSpeedFilters=rewrite_images"
URL="$REWRITE_URL,convert_jpeg_to_webp&$IMAGES_QUALITY=75&$WEBP_QUALITY=65"
fetch_until -save -recursive $URL \
  'fgrep -c 256x192xPuzzle.jpg.pagespeed.ic' 1 \
   --user-agent=webp
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd

rm -rf $WGET_DIR
fetch_until -save -recursive $URL \
  'fgrep -c 256x192xPuzzle.jpg.pagespeed.ic' 1 \
  --header='Accept:image/webp'
check_file_size "$WGET_DIR/*256x192*Puzzle*webp" -le 5140   # resized, webp'd
