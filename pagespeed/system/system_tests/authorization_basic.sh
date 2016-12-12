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
start_test authorized resources do not get cached and optimized.
URL="$TEST_ROOT/auth/medium_purple.css"
AUTH="Authorization:Basic dXNlcjE6cGFzc3dvcmQ="
not_cacheable_start=$(scrape_stat ipro_recorder_not_cacheable)
echo $WGET_DUMP --header="$AUTH" "$URL"
OUT=$($WGET_DUMP --header="$AUTH" "$URL")
check_from "$OUT" fgrep -q 'background: MediumPurple;'
not_cacheable=$(scrape_stat ipro_recorder_not_cacheable)
check [ $not_cacheable = $((not_cacheable_start + 1)) ]
URL=""
AUTH=""
