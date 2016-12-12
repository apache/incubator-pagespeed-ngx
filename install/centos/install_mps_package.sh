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
#
# Author: cheesy@google.com (Steve Hill)
#
# Remove old mod_pagespeed debs and install a new one.

pkg="$1"

echo Purging old releases...
# rpm --erase only succeeds if all packages listed are installed, so we need
# to find which one is installed and only erase that.
rpm --query mod-pagespeed-stable mod-pagespeed-beta | \
    grep -v "is not installed" | \
    xargs --no-run-if-empty sudo rpm --erase

echo "Installing $pkg..."
rpm --install "$pkg"
