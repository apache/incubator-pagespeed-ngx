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
# Test that we do not rewrite resources when the X-Sendfile header is set, or
# when the X-Accel-Redirect header is set.
start_test check that rewriting only happens without X-Sendfile
function verify_no_rewriting_sendfile() {
  local sendfile_hostname=$1
  local sendfile_header=$2
  URL="http://${sendfile_hostname}.example.com/mod_pagespeed_test/normal.js"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    --header 'X-PSA-Blocking-Rewrite: psatest' --save-headers $URL)
  check_from "$OUT" grep $sendfile_header
  check_from "$OUT" grep comment2
}
verify_no_rewriting_sendfile "uses-sendfile" "X-Sendfile"
verify_no_rewriting_sendfile "uses-xaccelredirect" "X-Accel-Redirect"
# doesnt-sendfile.example.com has identical configuration, but just does not
# set the X-Sendfile header. Check this here to make sure that we have do
# rewrite under other circumstances.
URL="http://doesnt-sendfile.example.com/mod_pagespeed_test/normal.js"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    'fgrep -c comment2' 0
