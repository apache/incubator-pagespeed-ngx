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
start_test vhost inheritance works
echo $WGET_DUMP $SECONDARY_CONFIG_URL
SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
config_title="<title>PageSpeed Configuration</title>"
check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
# Sharding is applied in this host, thanks to global inherit flag.
check_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

# We should also inherit the blocking rewrite key.
check_from "$SECONDARY_CONFIG" egrep -q "\(blrw\)[[:space:]]+psatest"
