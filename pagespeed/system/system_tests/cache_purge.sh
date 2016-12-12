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
start_test Cache purge test
cache_purge_test http://purge.example.com

# Run a simple cache_purge test but in a vhost with ModPagespeed off, and
# a subdirectory with htaccess file turning it back on, addressing
# https://github.com/pagespeed/mod_pagespeed/issues/1077
#
# TODO(jefftk): Uncomment this and delete uncomment the same test in
# apache/system_test.sh once nginx_system_test suppressions &/or
# "pagespeed off;" in server block allow location-overrides in ngx_pagespeed.
# See https://github.com/pagespeed/ngx_pagespeed/issues/968
# start_test Cache purging with PageSpeed off in vhost, but on in directory.
# cache_purge_test http://psoff-dir-on.example.com
