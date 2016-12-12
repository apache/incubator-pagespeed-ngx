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
# Rewrite images in styles.
start_test rewrite_images,rewrite_css,rewrite_style_attributes_with_url optimizes images in style.
FILE=rewrite_style_attributes.html?PageSpeedFilters=rewrite_images,rewrite_css,rewrite_style_attributes_with_url
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
fetch_until $URL 'grep -c BikeCrashIcn.png.pagespeed.ic.' 1
check run_wget_with_args $URL

# Now check that it can handle two of the same image in the same style block:
start_test two images in the same style block
FILE="rewrite_style_attributes_dual.html?PageSpeedFilters="
FILE+="rewrite_images,rewrite_css,rewrite_style_attributes_with_url"
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
PATTERN="BikeCrashIcn.png.pagespeed.ic.*BikeCrashIcn.png.pagespeed.ic"
fetch_until $URL "grep -c $PATTERN" 1
check run_wget_with_args $URL
