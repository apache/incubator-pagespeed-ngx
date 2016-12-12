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
test_filter add_instrumentation adds 2 script tags
check run_wget_with_args $URL
# Counts occurances of '<script' in $FETCHED
# See: http://superuser.com/questions/339522
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 2 ]

start_test "We don't add_instrumentation if URL params tell us not to"
FILE=add_instrumentation.html?PageSpeedFilters=
URL=$EXAMPLE_ROOT/$FILE
FETCHED=$WGET_DIR/$FILE
check run_wget_with_args $URL
check [ $(fgrep -o '<script' $FETCHED | wc -l) -eq 0 ]

# http://github.com/pagespeed/mod_pagespeed/issues/170
start_test "Make sure 404s aren't rewritten"
# Note: We run this in the add_instrumentation section because that is the
# easiest to detect which changes every page
THIS_BAD_URL=$BAD_RESOURCE_URL?PageSpeedFilters=add_instrumentation
# We use curl, because wget does not save 404 contents
OUT=$($CURL --silent $THIS_BAD_URL)
check_not_from "$OUT" fgrep "/mod_pagespeed_beacon"
