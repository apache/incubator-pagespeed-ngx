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
start_test Filters do not rewrite blacklisted JavaScript files.
URL=$TEST_ROOT/blacklist/blacklist.html?PageSpeedFilters=extend_cache,rewrite_javascript,trim_urls
fetch_until -save $URL 'grep -c .js.pagespeed.' 4
FETCHED=$FETCH_UNTIL_OUTFILE
check grep -q "<script src=\".*normal\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\"js_tinyMCE\.js\"></script>" $FETCHED
check grep -q "<script src=\"tiny_mce\.js\"></script>" $FETCHED
check grep -q "<script src=\"tinymce\.js\"></script>" $FETCHED
check grep -q \
  "<script src=\"scriptaculous\.js?load=effects,builder\"></script>" $FETCHED
check grep -q "<script src=\".*jquery.*\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\".*ckeditor\.js\">" $FETCHED
check grep -q "<script src=\".*swfobject\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q \
  "<script src=\".*another_normal\.js\.pagespeed\..*\.js\">" $FETCHED
