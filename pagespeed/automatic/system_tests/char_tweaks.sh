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
test_filter collapse_whitespace removes whitespace, but not from pre tags.
check run_wget_with_args $URL
check [ $(egrep -c '^ +<' $FETCHED) -eq 1 ]

test_filter pedantic adds default type attributes.
check run_wget_with_args $URL
check fgrep -q 'text/javascript' $FETCHED # should find script type
check fgrep -q 'text/css' $FETCHED        # should find style type

test_filter remove_comments removes comments but not IE directives.
check run_wget_with_args $URL
check_not grep removed $FETCHED   # comment, should not find
check grep -q preserved $FETCHED  # preserves IE directives

test_filter remove_quotes does what it says on the tin.
check run_wget_with_args $URL
num_quoted=$(sed 's/ /\n/g' $FETCHED | grep -c '"')
check [ $num_quoted -eq 2 ]       # 2 quoted attrs
check_not grep -q "'" $FETCHED    # no apostrophes

test_filter trim_urls makes urls relative
check run_wget_with_args $URL
check_not grep "mod_pagespeed_example" $FETCHED  # base dir, shouldn't find
check_file_size $FETCHED -lt 153  # down from 157
