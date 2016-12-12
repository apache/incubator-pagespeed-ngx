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
start_test "shortcut icons aren't inlined"

URL="$TEST_ROOT/shortcut_icons.html?PageSpeedFilters=debug,rewrite_images"

# We expect to see just one inline image, because of the img tag.
fetch_until -save $URL 'grep -c data:image/png' 1

OUT=$(cat $FETCH_UNTIL_OUTFILE)

# All four icon link tags should be optimized but not inlined.
check_from "$OUT" grep \
  '<link rel="icon" href="[^<]*xCuppa.png.pagespeed.ic'
check_from "$OUT" grep \
  '<link rel="apple-touch-icon" href="[^<]*xCuppa.png.pagespeed.ic'
check_from "$OUT" grep \
  '<link rel="apple-touch-icon-precomposed" href="[^<]*xCuppa.png.pagespeed.ic'
check_from "$OUT" grep \
  '<link rel="apple-touch-startup-image" href="[^<]*xCuppa.png.pagespeed.ic'

# The image tag should have been inlined instead.
check_not_from "$OUT" grep \
  '<img src="[^<]*xCuppa.png.pagespeed.ic'

function appears_exactly_four_times() {
  test $(grep -c "$@") = 4
}

check_from "$OUT" appears_exactly_four_times \
  "The image was not inlined because it is a shortcut icon."
