#!/bin/sh
#
# Copyright 2013 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: morlovich@google.com (Maksim Orlovich)
#
# A very simple wrapper around system 'ar' that drops the T (thin archive)
# option from archive commands, as gyp always put it in on Linux, while
# our build machines don't have it.

if [ "$1" = "crsT" ]; then
  shift 1
  exec /usr/bin/ar crs "$@"
else
  exec /usr/bin/ar "$@"
fi
