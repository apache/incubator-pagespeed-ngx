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
start_test json keeps its content type
URL="$TEST_ROOT/example.json"
OUT=$($WGET_DUMP "$URL?PageSpeed=off")
# Verify that it's application/json without PageSpeed touching it.
check_from "$OUT" grep '^Content-Type: application/json'
OUT=$($WGET_DUMP "$URL")
# Verify that it's application/json on the first PageSpeed load.
check_from "$OUT" grep '^Content-Type: application/json'
# Fetch it repeatedly until it's been IPRO-optimized.  This grep command is kind
# of awkward, because fetch_until doesn't do quoting well.
WGET_ARGS="--save-headers" fetch_until -save "$URL" \
  "grep -c .title.:.example.json" 1
OUT=$(cat $FETCH_UNTIL_OUTFILE)
# Make sure we didn't change the content type to application/javascript.
check_from "$OUT" grep '^Content-Type: application/json'
