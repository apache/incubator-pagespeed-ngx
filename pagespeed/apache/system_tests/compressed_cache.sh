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
function scrape_secondary_stat {
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    "$SECONDARY_ROOT/mod_pagespeed_statistics/" | \
    scrape_pipe_stat "$1"
}

if [ $statistics_enabled = "1" ]; then
  start_test CompressedCache is racking up savings on the root vhost
  original_size=$(scrape_stat compressed_cache_original_size)
  compressed_size=$(scrape_stat compressed_cache_compressed_size)
  echo original_size=$original_size compressed_size=$compressed_size
  check [ "$compressed_size" -lt "$original_size" ];
  check [ "$compressed_size" -gt 0 ];
  check [ "$original_size" -gt 0 ];

  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    start_test CompressedCache is turned off for the secondary vhost
    original_size=$(scrape_secondary_stat compressed_cache_original_size)
    compressed_size=$(scrape_secondary_stat compressed_cache_compressed_size)
    check [ "$compressed_size" -eq 0 ];
    check [ "$original_size" -eq 0 ];
  fi
else
  echo skipping CompressedCache test because stats is $statistics_enabled
fi
