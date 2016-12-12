#!/bin/bash
#
# Copyright 2013 Google Inc.
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
#
# Converts pagespeed_libraries.conf from Apache-format to Nginx-format,
# supporting the canonicalize_javascript_libraries filter.
# Inspired by https://github.com/pagespeed/ngx_pagespeed/issues/532
#
# Usage:
#   scripts/pagespeed_libraries_generator.sh > pagespeed_libraries.conf
#
# Then have nginx include that configuration file and enable the filter
# canonicalize_javascript_libraries.
#
# Author: vid@zippykid.com (Vid Luther)
#         jefftk@google.com (Jeff Kaufman)

URL="https://github.com/pagespeed/mod_pagespeed/raw/master/"
URL+="net/instaweb/genfiles/conf/pagespeed_libraries.conf"
curl -L -s -S "$URL" \
    | grep ModPagespeedLibrary \
    | while read library size hash url ; do
  echo "  pagespeed Library $size $hash $url;"
done
