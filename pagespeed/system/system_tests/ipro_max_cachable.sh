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
start_test max cacheable content length with ipro
URL="http://max-cacheable-content-length.example.com/mod_pagespeed_example/"
URL+="images/BikeCrashIcn.png"
# This used to check-fail the server; see ngx_pagespeed issue #771.
http_proxy=$SECONDARY_HOSTNAME check $WGET -t 1 -O /dev/null $URL
