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
start_test move_css_above_scripts works.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=move_css_above_scripts
FETCHED=$OUTDIR/output-file
$WGET_DUMP $URL > $FETCHED
# Link moved before script.
check grep -q "styles/all_styles.css\"><script" $FETCHED

start_test move_css_above_scripts off.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved before script.
check_not grep "styles/all_styles.css\"><script" $FETCHED

start_test move_css_to_head does what it says on the tin.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=move_css_to_head
$WGET_DUMP $URL > $FETCHED
# Link moved to head.
check grep -q "styles/all_styles.css\"></head>" $FETCHED

start_test move_css_to_head off.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved to head.
check_not grep "styles/all_styles.css\"></head>" $FETCHED
