#!/bin/sh
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
#
# Deletes the specified cache directories and the apache log.  Note that this
# is not necessary or sufficient for purging the cache in a running system,
# because it does not affect the in-memory cache, or any external caches like
# memcached or redis.  This is used for ensuring a clean slate at the start
# of tests.
#
# This reads environment variables APACHE_LOG and MOD_PAGESPEED_CACHE, which
# are exported by pagespeed/install/Makefile.tests.

set -u
set -e

(set -x; rm -f "$APACHE_LOG")

dir_count=0
for dir in "$MOD_PAGESPEED_CACHE"*; do
  dir_count=$((dir_count + 1))
done

i=0
for dir in "$MOD_PAGESPEED_CACHE"*; do
  i=$((i + 1))
  echo rm -rf "$dir" "($i/$dir_count)"
  rm -rf "$dir"
done
