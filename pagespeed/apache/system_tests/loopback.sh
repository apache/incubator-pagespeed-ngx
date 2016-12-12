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
# Test that loopback route fetcher works with vhosts not listening on
# 127.0.0.1  Only run this during CACHE_FLUSH_TEST as that is when
# APACHE_TERTIARY_PORT is set.
if [ "${APACHE_TERTIARY_PORT:-}" != "" ]; then
  start_test IP choice for loopback fetches.
  HOST_NAME="loopbackfetch.example.com"
  URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
  http_proxy=127.0.0.2:$APACHE_TERTIARY_PORT \
    fetch_until $URL 'grep -c .pagespeed.ic' 2
fi
