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
test_filter responsive_images,rewrite_images,-inline_images adds srcset for \
  Puzzle.jpg and Cuppa.png
fetch_until $URL 'grep -c srcset=' 3
# Make sure all Puzzle URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Puzzle.jpg' 0
check egrep -q 'xPuzzle.jpg.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xPuzzle.jpg.pagespeed.ic.[0-9a-zA-Z_-]+.jpg [0-9.]+x,?)+"' $FETCH_FILE
# Make sure all Cuppa URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Cuppa.png' 0
check egrep -q 'xCuppa.png.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xCuppa.png.pagespeed.ic.[0-9a-zA-Z_-]+.png [0-9.]+x,?)+"' $FETCH_FILE

test_filter responsive_images,rewrite_images,+inline_images adds srcset for \
  Puzzle.jpg, but not Cuppa.png
# Cuppa.png will be inlined, so we should not get a srcset for it.
fetch_until $URL 'grep -c Cuppa.png' 0  # Make sure Cuppa.png is inlined.
fetch_until $URL 'grep -c srcset=' 2    # And only two srcsets (for Puzzle.jpg).
# Make sure all Puzzle URLs are rewritten.
fetch_until -save $URL 'grep -c [^x]Puzzle.jpg' 0
check egrep -q 'xPuzzle.jpg.pagespeed.+srcset="([^ ]*images/([0-9]+x[0-9]+)?xPuzzle.jpg.pagespeed.ic.[0-9a-zA-Z_-]+.jpg [0-9.]+x,?)+"' $FETCH_FILE

start_test rewrite_images can rewrite srcset itself
URL=$TEST_ROOT/image_rewriting/srcset.html?PageSpeedFilters=+rewrite_images,+debug
fetch_until -save $URL 'grep -c xPuzzle.*1x.*xCuppa.*2x' 1


