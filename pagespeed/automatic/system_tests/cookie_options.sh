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
# Test that we can set options using cookies.
start_test Cookie options on: by default comments not removed, whitespace is
URL="$(generate_url options-by-cookies-enabled.example.com \
                    /mod_pagespeed_test/forbidden.html)"
echo  http_proxy=$SECONDARY_HOSTNAME wget $URL
echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
check_from     "$OUT" grep -q '<!--'
check_not_from "$OUT" grep -q '  '

start_test Cookie options on: set option by cookie takes effect
echo wget --header=Cookie:PageSpeedFilters=%2bremove_comments $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
       --header=Cookie:PageSpeedFilters=%2bremove_comments $URL)"
check_not_from "$OUT" grep -q '<!--'
check_not_from "$OUT" grep -q '  '

start_test Cookie options on: invalid cookie does not take effect
# The '+' must be encoded as %2b for the cookie parsing code to accept it.
echo wget --header=Cookie:PageSpeedFilters=+remove_comments $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
       --header=Cookie:PageSpeedFilters=+remove_comments $URL)"
check_from     "$OUT" grep -q '<!--'
check_not_from "$OUT" grep -q '  '

start_test Cookie options off: by default comments nor whitespace removed
URL="$(generate_url options-by-cookies-disabled.example.com \
                    /mod_pagespeed_test/forbidden.html)"
echo wget $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
check_from "$OUT" grep -q '<!--'
check_from "$OUT" grep -q '  '

start_test Cookie options off: set option by cookie has no effect
echo wget --header=Cookie:PageSpeedFilters=%2bremove_comments $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
       --header=Cookie:PageSpeedFilters=%2bremove_comments $URL)"
check_from "$OUT" grep -q '<!--'
check_from "$OUT" grep -q '  '
