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
start_test In-place resource optimization
FETCHED=$OUTDIR/ipro
# Note: we intentionally want to use an image which will not appear on
# any HTML pages, and thus will not be in cache before this test is run.
# (Since the system_test is run multiple times without clearing the cache
# it may be in cache on some of those runs, but we know that it was put in
# the cache by previous runs of this specific test.)
URL=$TEST_ROOT/ipro/test_image_dont_reuse.png
# Size between original image size and rewritten image size (in bytes).
# Used to figure out whether the returned image was rewritten or not.
THRESHOLD_SIZE=13000

# Check that we compress the image (with IPRO).
# Note: This requests $URL until it's size is less than $THRESHOLD_SIZE.
fetch_until -save $URL "wc -c" $THRESHOLD_SIZE "--save-headers" "-lt"
check_file_size $FETCH_FILE -lt $THRESHOLD_SIZE
# Check that resource is served with small Cache-Control header (since
# we cannot cache-extend resources served under the original URL).
# Note: tr -d '\r' is needed because HTTP spec requires lines to end in \r\n,
# but sed does not treat that as $.
echo sed -n 's/Cache-Control: max-age=\([0-9]*\)$/\1/p' $FETCH_FILE
check [ "$(tr -d '\r' < $FETCH_FILE | \
           sed -n 's/Cache-Control: max-age=\([0-9]*\)$/\1/p')" \
        -lt 1000 ]

# Check that the original image is greater than threshold to begin with.
check $WGET_DUMP -O $FETCHED $URL?PageSpeed=off
check_file_size $FETCHED -gt $THRESHOLD_SIZE
