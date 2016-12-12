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
start_test can combine css with authorized ids only
URL="$TEST_ROOT/combine_css_with_ids.html?PageSpeedFilters=combine_css"
# Test big.css and bold.css are combined, but not yellow.css or blue.css.
fetch_until -save "$URL" 'fgrep -c styles/big.css+bold.css.pagespeed.cc' 1
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/yellow.css" id='
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/blue.css" id='
