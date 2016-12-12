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
start_test respect vary user-agent
URL="$SECONDARY_HOSTNAME/mod_pagespeed_test/vary/index.html"
URL+="?PageSpeedFilters=inline_css"
FETCH_CMD="$WGET_DUMP --header=Host:respectvary.example.com $URL"
OUT=$($FETCH_CMD)
# We want to verify that css is not inlined, but if we just check once then
# pagespeed doesn't have long enough to be able to inline it.
sleep .1
OUT=$($FETCH_CMD)
check_not_from "$OUT" fgrep "<style>"
