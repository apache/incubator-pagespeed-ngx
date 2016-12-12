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
start_test Ajax overrides experiments

# Normally, we expect this <head>less HTML file to have a <head> added
# by the experimental infrastructure, and for the double-space between
# "two  spaces" to be collapsed to one, due to the experiment.
URL="http://experiment.ajax.example.com/mod_pagespeed_test/ajax/ajax.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
  'fgrep -c .pagespeed.' 3
OUT=$(cat "$FETCH_FILE")
check_from "$OUT" fgrep -q '<head'
check_from "$OUT" fgrep -q 'Two spaces.'

# However, we must not add a '<head' in an Ajax request, rewrite any URLs, or
# execute the collapse_whitespace experiment.
start_test Experiments not injected on ajax.html with an Ajax header
AJAX="--header=X-Requested-With:XmlHttpRequest"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save -expect_time_out "$URL" \
  'fgrep -c \.pagespeed\.' 3  "$AJAX"
OUT=$(cat "$FETCH_FILE")
check_not_from "$OUT" fgrep -q '<head'
check_not_from "$OUT" fgrep -q '.pagespeed.'
check_from "$OUT" fgrep -q 'Two  spaces.'

start_test Ajax disables any filters that add head.

# While we are in here, check also that Ajax requests don't get a 'head',
# even if we are not in an experiment.
URL="http://ajax.example.com/mod_pagespeed_test/ajax/ajax.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
  'fgrep -c .pagespeed.' 3
OUT=$(cat "$FETCH_FILE")
check_from "$OUT" fgrep -q '<head'

# However, we must not add a '<head' in an Ajax request or rewrite any URLs.
http_proxy=$SECONDARY_HOSTNAME fetch_until -save -expect_time_out "$URL" \
  'fgrep -c \.pagespeed\.' 3  "$AJAX"
OUT=$(cat "$FETCH_FILE")
check_not_from "$OUT" fgrep -q '<head'
check_not_from "$OUT" fgrep -q '.pagespeed.'
