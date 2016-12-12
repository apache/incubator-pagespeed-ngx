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
# Check all the pagespeed_admin pages, both in its default location and an
# alternate.
start_test pagespeed_admin and alternate_admin_path
function check_admin_banner() {
  path="$1"
  title="$2"
  tmpfile=$TESTTMP/admin.html
  echo $WGET_DUMP $PRIMARY_SERVER/$path '>' $tmpfile ...
  $WGET_DUMP $PRIMARY_SERVER/$path > $tmpfile
  check fgrep -q "<title>PageSpeed $title</title>" $tmpfile
  rm -f $tmpfile
}
for admin_path in pagespeed_admin pagespeed_global_admin alt/admin/path; do
  check_admin_banner $admin_path/statistics "Statistics"
  check_admin_banner $admin_path/config "Configuration"
  check_admin_banner $admin_path/histograms "Histograms"
  check_admin_banner $admin_path/cache "Caches"
  check_admin_banner $admin_path/console "Console"
  check_admin_banner $admin_path/message_history "Message History"
done

start_test pagespeed_admin quoting on not-found page
OUT=$($CURL $PRIMARY_SERVER/'pagespeed_admin/a/<boo>')
check_not_from "$OUT" fgrep -q "<boo>"
check_from "$OUT" fgrep -q "%3Cboo%3E"

# Note that query params are stripped completely
OUT=$($CURL $PRIMARY_SERVER/'pagespeed_admin/a?<boo>')
check_not_from "$OUT" fgrep -q "boo"
