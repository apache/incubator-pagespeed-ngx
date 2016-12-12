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
# Test to make sure we have a sane Connection Header.  See
# https://github.com/pagespeed/mod_pagespeed/issues/664
#
# Note that this bug is dependent on seeing a resource for the first time in
# the InPlaceResourceOptimization path, because in that flow we are caching
# the response-headers from the server.  The reponse-headers from Serf never
# seem to include the Connection header.  So we have to cachebust the JS file.
start_test Sane Connection header
URL="$TEST_ROOT/normal.js?q=cachebust"
fetch_until -save $URL 'grep -c W/\"PSA-aj-' 1 --save-headers
CONNECTION=$(extract_headers $FETCH_UNTIL_OUTFILE | fgrep "Connection:")
check_not_from "$CONNECTION" fgrep -qi "Keep-Alive, Keep-Alive"
check_from "$CONNECTION" fgrep -qi "Keep-Alive"
