#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework (furious) tests that depend on
# ModPagespeedAnalyticsID being set.
#
# See apache_furious_test for usage.
#
this_dir=$(dirname $0)
source "$this_dir/apache_furious_test.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"

start_test Analytics javascript is added for the experimental group.
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=2' $EXTEND_CACHE)
check_from "$OUT" fgrep -q 'Experiment: 2'
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=7' $EXTEND_CACHE)
check_from "$OUT" fgrep -q 'Experiment: 7'

start_test Analytics javascript is not added for the no-experiment group.
OUT=$($WGET_DUMP --header='Cookie: _GFURIOUS=0' $EXTEND_CACHE)
check_not_from "$OUT" fgrep -q 'Experiment:'

system_test_trailer
