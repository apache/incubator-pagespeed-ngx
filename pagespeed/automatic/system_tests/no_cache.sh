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
echo Test that we can rewrite resources that are served with
echo Cache-Control: no-cache with on-the-fly filters.  Tests that the
echo no-cache header is preserved.
test_filter extend_cache with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.ce.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.ce.0.js
check fgrep -q "no-cache" $WGET_OUTPUT

echo Test that we can rewrite Cache-Control: no-cache resources with
echo non-on-the-fly filters.
test_filter rewrite_javascript with no-cache js origin
URL="$REWRITTEN_TEST_ROOT/no_cache/hello.js.pagespeed.jm.0.js"
echo run_wget_with_args $URL
run_wget_with_args $URL
check fgrep -q "'Hello'" $WGET_DIR/hello.js.pagespeed.jm.0.js
check fgrep -q "no-cache" $WGET_OUTPUT
