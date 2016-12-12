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
start_test IPRO-optimized resources should have fixed size, not chunked.
URL="$EXAMPLE_ROOT/images/Puzzle.jpg"
URL+="?PageSpeedJpegRecompressionQuality=75"
fetch_until -save $URL "wc -c" 90000 "--save-headers" "-lt"
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" grep -q '^Content-Length:'
CONTENT_LENGTH=$(extract_headers $FETCH_UNTIL_OUTFILE | \
  grep '^Content-Length:' | awk '{print $2}')
check [ "$CONTENT_LENGTH" -lt 90000 ];
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -q 'Transfer-Encoding: chunked'

start_test IPRO 304 with etags
# Reuses $URL and $FETCH_UNTIL_OUTFILE from previous test.
case_normalized_headers=$(
  extract_headers $FETCH_UNTIL_OUTFILE | sed 's/^Etag:/ETag:/')
check_from "$case_normalized_headers" fgrep -q 'ETag:'
ETAG=$(echo "$case_normalized_headers" | awk '/ETag:/ {print $2}')
echo $WGET_DUMP --header "If-None-Match: $ETAG" $URL
OUTFILE=$OUTDIR/etags
# Note: -o gets debug info which is the only place that 304 message is sent.
check_not $WGET -o $OUTFILE -O /dev/null --header "If-None-Match: $ETAG" $URL
check fgrep -q "awaiting response... 304" $OUTFILE

