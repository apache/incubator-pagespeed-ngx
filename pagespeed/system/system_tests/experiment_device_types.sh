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
readonly EXP_DEVICES_EXAMPLE="http://experiment.devicematch.example.com/mod_pagespeed_example"
readonly EXP_DEVICES_EXTEND_CACHE="$EXP_DEVICES_EXAMPLE/extend_cache.html"

readonly DESKTOP_UA="Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) AppleWebKit/534.13 (KHTML, like Gecko) Chrome/18.0.597.19 Safari/534.13"
readonly MOBILE_UA="Mozilla/5.0 (Linux; Android 4.1.4; Galaxy Nexus Build/IMM76B) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/21.0.1025.133 Mobile Safari/535.19"

start_test Mobile experiment does not match desktop device.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$DESKTOP_UA" \
      $EXP_DEVICES_EXTEND_CACHE)
check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=0;'

start_test Mobile experiment matches mobile device.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$MOBILE_UA" \
      $EXP_DEVICES_EXTEND_CACHE)
check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=1;'

start_test Can force-enroll in experment for wrong device type.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$DESKTOP_UA" \
      $EXP_DEVICES_EXTEND_CACHE?PageSpeedEnrollExperiment=1)
check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=1;'
