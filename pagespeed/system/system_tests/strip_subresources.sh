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
start_test Strip subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip multiple subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/multiple_subresource_hints.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources default behaviour disallow
URL="$TEST_ROOT/strip_subresource_hints/default/disallowtest.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve on
URL="$TEST_ROOT/strip_subresource_hints/preserve_on/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve off
URL="$TEST_ROOT/strip_subresource_hints/preserve_off/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources rewrite level passthrough
URL="$TEST_ROOT/strip_subresource_hints/default_passthrough/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"
