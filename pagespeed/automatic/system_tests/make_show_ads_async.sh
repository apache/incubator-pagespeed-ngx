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
test_filter make_show_ads_async works
OUT=$($WGET_DUMP $URL)
check_from     "$OUT" grep -q 'data-ad'
check_not_from "$OUT" grep -q 'google_ad'
check_from     "$OUT" grep -q 'adsbygoogle.js'
check_not_from "$OUT" grep -q 'show_ads.js'
check_from     "$OUT" fgrep -q "<script>(adsbygoogle = window.adsbygoogle || []).push({})</script>"
