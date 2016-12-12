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
# None of these tests apply to nginx because it doesn't echo back urls or log
# them in these cases.

# Test error handler quoting.  We use curl, because wget does not save
# 404 contents.
start_test Proper quoting in our 404 handler
EVIL_PATH="404<evil>.js.pagespeed.jm.0.js"
SANITIZED_PATH="404&lt;evil&gt;.js.pagespeed.jm.0.js"
EVIL_URL="$HOSTNAME/$EVIL_PATH"
OUT=$($CURL --silent $EVIL_URL)
check_not_from "$OUT" fgrep -q "$EVIL_PATH"
check_from "$OUT" fgrep -q "$SANITIZED_PATH"

# Test mod_pagespeed_message quoting.  The above test will have injected
# $EVIL_URL accurately into the log, but should be sanitized in
# mod_pagespeed_message.
start_test Proper quoting in mod_pagespeed_message
MSG="$HOSTNAME/mod_pagespeed_message"
function check_sanitized() {
  tail -60 | fgrep -c "$SANITIZED_PATH"
}
fetch_until -save "$MSG" check_sanitized 1
OUT=$($WGET -q -O - $HOSTNAME/mod_pagespeed_message)
check_not fgrep -q "$EVIL_PATH" "$FETCH_FILE"
check grep -q "$EVIL_URL" "$APACHE_LOG"

# Test static handler quoting.
start_test Proper quoting in $PSA_JS_LIBRARY_URL_PREFIX
EVIL_PATH="$PSA_JS_LIBRARY_URL_PREFIX/<evil>.js"
SANITIZED_PATH="$PSA_JS_LIBRARY_URL_PREFIX/&lt;evil&gt;.js"
OUT=$($CURL --silent "$HOSTNAME/$EVIL_PATH")
check_not_from "$OUT" fgrep -q "$EVIL_PATH"
check_from "$OUT" fgrep -q "$SANITIZED_PATH"
