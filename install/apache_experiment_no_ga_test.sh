#!/bin/bash
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
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework tests that depend on AnalyticsID
# being unset.
#
# See apache_experiment_test for usage.
#
this_dir=$(dirname $0)
source "$this_dir/apache_experiment_test.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"

start_test Analytics javascript is not added for any group.
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=2' $EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=7' $EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=0' $EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'

check_failures_and_exit
