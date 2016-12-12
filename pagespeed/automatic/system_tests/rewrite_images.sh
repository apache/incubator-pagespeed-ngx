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
test_filter rewrite_images inlines, compresses, and resizes.
fetch_until $URL 'grep -c data:image/png' 1  # Images inlined.
fetch_until $URL 'grep -c .pagespeed.ic' 2  # Images rewritten.

# Verify with a blocking fetch that data-pagespeed-no-transform worked and was
# stripped.
fetch_until $URL 'grep -c "images/disclosure_open_plus.png"' 1 \
  '--header=X-PSA-Blocking-Rewrite:psatest'
fetch_until $URL 'grep -c "data-pagespeed-no-transform"' 0 \
  '--header=X-PSA-Blocking-Rewrite:psatest'

start_test size of rewritten image
# Note: We cannot do this above because the intervening fetch_untils will
# clean up $OUTDIR.
fetch_until -save -recursive $URL 'grep -c .pagespeed.ic' 2
check_file_size "$WGET_DIR/xBikeCrashIcn*" -lt 25000      # re-encoded
check_file_size "$WGET_DIR/*256x192*Puzzle*" -lt 24126    # resized
URL=$EXAMPLE_ROOT"/rewrite_images.html?PageSpeedFilters=rewrite_images"

IMG_URL=$(egrep -o 'http://[^"]*pagespeed.[^"]*.jpg' $FETCHED | head -n1)
if [ -z "$IMG_URL" ]; then
  # If PreserveUrlRelativity is on, we need to find the relative URL and
  # absolutify it ourselves.
  IMG_URL="$EXAMPLE_ROOT/"
  IMG_URL+=$(grep -o '[^\"]*pagespeed.[^\"]*\.jpg' $FETCHED | head -n 1)
fi

start_test headers for rewritten image
echo "$IMG_URL"
IMG_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $IMG_URL 2>&1)
check_200_http_response "$IMG_HEADERS"
# Make sure we have some valid headers.
check_from "$IMG_HEADERS" fgrep -qi 'Content-Type: image/jpeg'
# Make sure the response was not gzipped.
start_test Images are not gzipped.
check_not_from "$IMG_HEADERS" fgrep -i 'Content-Encoding: gzip'
# Make sure there is no vary-encoding
start_test Vary is not set for images.
check_not_from "$IMG_HEADERS" fgrep -i 'Vary: Accept-Encoding'
# Make sure there is an etag
start_test Etags is present.
check_from "$IMG_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
# Make sure an extra header is propagated from input resource to output
# resource.  X-Extra-Header is added in debug.conf.template.
start_test Extra header is present
check_from "$IMG_HEADERS" fgrep -qi 'X-Extra-Header'
# Make sure there is a last-modified tag
start_test Last-modified is present.
check_from "$IMG_HEADERS" fgrep -qi 'Last-Modified'
