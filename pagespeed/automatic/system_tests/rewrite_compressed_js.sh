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
echo Testing whether we can rewrite javascript resources that are served
echo gzipped, even though we generally ask for them clear.  This particular
echo js file has "alert('Hello')" but is checked into source control in gzipped
echo format and served with the gzip headers, so it is decodable.  This tests
echo that we can inline and minify that file.
test_filter rewrite_javascript,inline_javascript with gzipped js origin
URL="$TEST_ROOT/rewrite_compressed_js.html"
QPARAMS="PageSpeedFilters=rewrite_javascript,inline_javascript"
fetch_until "$URL?$QPARAMS" "grep -c Hello'" 1
