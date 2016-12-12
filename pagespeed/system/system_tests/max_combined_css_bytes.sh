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
# TODO(sligocki): Following test only works with query_params. Fix to work
# with any method and get rid of this manual set.
filter_spec_method="query_params"
# Test for MaxCombinedCssBytes. The html used in the test, 'combine_css.html',
# has 4 CSS files in the following order.
#   yellow.css :   36 bytes
#   blue.css   :   21 bytes
#   big.css    : 4307 bytes
#   bold.css   :   31 bytes
# Because the threshold was chosen as '57', only the first two CSS files
# are combined.
test_filter combine_css Maximum size of combined CSS.
QUERY_PARAM="PageSpeedMaxCombinedCssBytes=57"
URL="$URL&$QUERY_PARAM"
# Make sure that we have exactly 3 CSS files (after combination).
fetch_until -save $URL 'grep -c text/css' 3
# Now check that the 1st and 2nd CSS files are combined, but the 3rd
# one is not.
check [ $(grep -c 'styles/yellow.css+blue.css.pagespeed.' \
    $FETCH_UNTIL_OUTFILE) = 1 ]
check [ $(grep -c 'styles/big.css\"' $FETCH_UNTIL_OUTFILE) = 1 ]
check [ $(grep -c 'styles/bold.css\"' $FETCH_UNTIL_OUTFILE) = 1 ]
