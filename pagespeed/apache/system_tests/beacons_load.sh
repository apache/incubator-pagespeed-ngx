#!/bin/bash
#
# Copyright 2012 Google Inc.
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
# This is dependent upon having a beacon handler.
test_filter add_instrumentation beacons load.
OUT=$($CURL -sSi $PRIMARY_SERVER/$BEACON_HANDLER?ets=load:13)
check_from "$OUT" fgrep -q "204 No Content"
check_from "$OUT" fgrep -q 'Cache-Control: max-age=0, no-cache'
