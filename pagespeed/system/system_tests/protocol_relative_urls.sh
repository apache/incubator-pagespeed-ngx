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
start_test Protocol relative urls
URL="$PRIMARY_SERVER//www.modpagespeed.com/"
URL+="styles/A.blue.css.pagespeed.cf.0.css"
OUT=$($CURL -D- -o/dev/null -sS "$URL")
check_from "$OUT" grep "^HTTP.* 404 Not Found"
