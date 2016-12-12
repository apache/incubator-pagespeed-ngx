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
# Fetch with the default limit so our test file is inlined.
test_filter flatten_css_imports,rewrite_css default limit
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check_not grep @import.url $FETCHED
check grep -q "yellow.background-color:" $FETCHED

# Fetch with a tiny limit so no file can be inlined.
test_filter flatten_css_imports,rewrite_css tiny limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:5"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED

# Fetch with a medium limit so any one file can be inlined but not all of them.
test_filter flatten_css_imports,rewrite_css medium limit
WGET_ARGS="${WGET_ARGS} --header=PageSpeedCssFlattenMaxBytes:50"
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args $URL
check run_wget_with_args $URL
check grep -q @import.url $FETCHED
check_not grep "yellow.background-color:" $FETCHED
