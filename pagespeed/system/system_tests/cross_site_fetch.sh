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
# Test lying host headers for cross-site fetch. This should fetch from localhost
# and therefore succeed.
start_test Lying host headers for cross-site fetch
EVIL_URL=$HOSTNAME/mod_pagespeed_example/styles/big.css.pagespeed.ce.8CfGBvwDhH.css
echo wget --save-headers -O - '--header=Host:www.google.com' $EVIL_URL
check wget --save-headers -O - '--header=Host:www.google.com' $EVIL_URL >& $TESTTMP/evil
rm -f $TESTTMP/evil
