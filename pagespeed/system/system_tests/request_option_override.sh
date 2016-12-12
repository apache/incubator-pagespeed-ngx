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
start_test Request Option Override : Correct values are passed
HOST_NAME="http://request-option-override.example.com"
OPTS="?ModPagespeed=on"
OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
OPTS+="&PageSpeedRequestOptionOverride=abc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
echo wget $URL
check_not_from "$OUT" grep -q '<!--'

start_test Request Option Override : Incorrect values are passed
HOST_NAME="http://request-option-override.example.com"
OPTS="?ModPagespeed=on"
OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
OPTS+="&PageSpeedRequestOptionOverride=notabc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
echo wget $URL
check_from "$OUT" grep -q '<!--'

start_test Request Option Override : Correct values are passed as headers
HOST_NAME="http://request-option-override.example.com"
OPTS="--header=ModPagespeed:on"
OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
OPTS+=" --header=PageSpeedRequestOptionOverride:abc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
echo wget $OPTS $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
check_not_from "$OUT" grep -q '<!--'

start_test Request Option Override : Incorrect values are passed as headers
HOST_NAME="http://request-option-override.example.com"
OPTS="--header=ModPagespeed:on"
OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
OPTS+=" --header=PageSpeedRequestOptionOverride:notabc"
URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
echo wget $OPTS $URL
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
check_from "$OUT" grep -q '<!--'
