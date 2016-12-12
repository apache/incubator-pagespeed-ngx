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
start_test rel-canonical

# .pagespeed. resources should have Link rel=canonical headers, IPRO resources
# should not have them.

start_test link rel=canonical header not present with IPRO resources

REL_CANONICAL_REGEXP='Link:.*rel.*canonical'

URL=$EXAMPLE_ROOT/images/Puzzle.jpg
# Fetch it a few times until IPRO is done and has given it an ipro ("aj") etag.
fetch_until -save "$URL" 'grep -c E[Tt]ag:.W/.PSA-aj.' 1 --save-headers
# rel=canonical should not be present.
check [ $(grep -c "$REL_CANONICAL_REGEXP" $FETCH_FILE) = 0 ]

start_test link rel=canonical header present with pagespeed.ce resources

URL=$REWRITTEN_ROOT/images/Puzzle.jpg.pagespeed.ce.HASH.jpg
OUT=$($CURL -D- -o/dev/null -sS $URL)
check_from "$OUT" grep "$REL_CANONICAL_REGEXP"

start_test link rel=canonical header present with pagespeed.ic resources

URL=$REWRITTEN_ROOT/images/xPuzzle.jpg.pagespeed.ic.HASH.jpg
OUT=$($CURL -D- -o/dev/null -sS  $URL)
check_from "$OUT" grep "$REL_CANONICAL_REGEXP"
