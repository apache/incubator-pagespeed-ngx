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
BAD_IMG_URL=$REWRITTEN_ROOT/images/xBadName.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
start_test rewrite_images fails broken image
echo run_wget_with_args $BAD_IMG_URL
check_not run_wget_with_args $BAD_IMG_URL  # fails
check grep "404 Not Found" $WGET_OUTPUT

start_test "rewrite_images doesn't 500 on unoptomizable image."
IMG_URL=$REWRITTEN_ROOT/images/xOptPuzzle.jpg.pagespeed.ic.Zi7KMNYwzD.jpg
run_wget_with_args -q $IMG_URL
check_200_http_response_file "$WGET_OUTPUT"
