#!/bin/bash
#
# Copyright 2012 Google Inc. All Rights Reserved.
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs all Apache-specific experiment framework tests that don't depend on
# Google Analytics.
#
# See automatic/system_test_helpers.sh for usage.
#
# Not intended to be run stand-alone.  Should be run only by
# apache_experiment_no_ge_test and apache_experiment_ga_test.
#

this_dir="$( dirname "${BASH_SOURCE[0]}" )"
INSTAWEB_CODE_DIR="$this_dir/../net/instaweb"
if [ ! -e "$INSTAWEB_CODE_DIR" ] ; then
  INSTAWEB_CODE_DIR="$this_dir/../../"
fi
source "$INSTAWEB_CODE_DIR/automatic/system_test_helpers.sh" || exit 1

EXAMPLE="$1/mod_pagespeed_example"
EXTEND_CACHE="$EXAMPLE/extend_cache.html"
MPS_TEST="$1/mod_pagespeed_test"
ARIS="$MPS_TEST/avoid_renaming_introspective_javascript__off.html"

echo Testing whether or not the experiment framework is working.
start_test PageSpeedExperiment cookie is set.
OUT=$($WGET_DUMP $EXTEND_CACHE)
check_from "$OUT" fgrep "PageSpeedExperiment="

start_test PageSpeedFilters query param should disable experiments.
OUT=$($WGET_DUMP "$EXTEND_CACHE?PageSpeed=on&PageSpeedFilters=rewrite_css")
check_not_from "$OUT" fgrep 'PageSpeedExperiment='

start_test ModPagespeedFilters query param should also disable experiments.
OUT=$($WGET_DUMP \
  "$EXTEND_CACHE?ModPagespeed=on&ModPagespeedFilters=rewrite_css")
check_not_from "$OUT" fgrep 'PageSpeedExperiment='

start_test experiment assignment can be forced
OUT=$($WGET_DUMP \
  "$EXTEND_CACHE?PageSpeedEnrollExperiment=2")
check_from "$OUT" fgrep 'PageSpeedExperiment=2'

start_test experiment assignment can be forced to a 0% experiment
OUT=$($WGET_DUMP \
  "$EXTEND_CACHE?PageSpeedEnrollExperiment=3")
check_from "$OUT" fgrep 'PageSpeedExperiment=3'

start_test experiment assignment can be forced even if already assigned
OUT=$($WGET_DUMP --header Cookie:PageSpeedExperiment=7 \
  "$EXTEND_CACHE?PageSpeedEnrollExperiment=2")
check_from "$OUT" fgrep 'PageSpeedExperiment=2'

start_test If the user is already assigned, no need to assign them again.
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=2' $EXTEND_CACHE)
check_not_from "$OUT" fgrep 'PageSpeedExperiment='

start_test The beacon should include the experiment id.
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=2' $EXTEND_CACHE)
check_from "$OUT" grep "pagespeed.addInstrumentationInit('/mod_pagespeed_beacon', 'load', '&exptid=2', 'http://localhost[:0-9]*/mod_pagespeed_example/extend_cache.html');"
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=7' $EXTEND_CACHE)
check_from "$OUT" grep "pagespeed.addInstrumentationInit('/mod_pagespeed_beacon', 'load', '&exptid=7', 'http://localhost[:0-9]*/mod_pagespeed_example/extend_cache.html');"

start_test The no-experiment group beacon should not include an experiment id.
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=0' $EXTEND_CACHE)
check_not_from "$OUT" grep 'mod_pagespeed_beacon.*exptid'

# We expect id=7 to be index=a and id=2 to be index=b because that's the
# order they're defined in the config file.
start_test Resource urls are rewritten to include experiment indexes.
WGET_ARGS="--header Cookie:PageSpeedExperiment=7" fetch_until $EXTEND_CACHE \
  "fgrep -c .pagespeed.a.ic." 1
WGET_ARGS="--header Cookie:PageSpeedExperiment=2" fetch_until $EXTEND_CACHE \
  "fgrep -c .pagespeed.b.ic." 1
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=7' $EXTEND_CACHE)
check_from "$OUT" fgrep ".pagespeed.a.ic."
OUT=$($WGET_DUMP --header='Cookie: PageSpeedExperiment=2' $EXTEND_CACHE)
check_from "$OUT" fgrep ".pagespeed.b.ic."

start_test Options are respected.
# For id 2 ARIS is on.  First fetch until normal.js is rewritten, after which
# we expect introspective.js would be rewritten if it were going to be.
WGET_ARGS="--header Cookie:PageSpeedExperiment=2" fetch_until -save $ARIS \
  'grep -c "src=\"normal.js\""' 0
check [ $(grep -c "src=\"introspection.js\"" $FETCH_UNTIL_OUTFILE) = 1 ]

# For id 7 ARIS is off.  Repeat this test, expecting it to get renamed.
WGET_ARGS="--header Cookie:PageSpeedExperiment=7" fetch_until -save $ARIS \
  'grep -c "src=\"normal.js\""' 0
check [ $(grep -c "src=\"introspection.js\"" $FETCH_UNTIL_OUTFILE) = 0 ]

start_test Images are different when the url specifies different experiments.
# While the images are the same, image B should be smaller because in the config
# file we enable convert_jpeg_to_progressive only for id=2 (side B).  Ideally we
# would check that it was actually progressive, by checking whether "identify
# -verbose filename" produced "Interlace: JPEG" or "Interlace: None", but that
# would introduce a dependency on imagemagick.  This is just as accurate, but
# more brittle (because changes to our compression code would change the
# computed file sizes).
IMG_A="$EXAMPLE/images/xPuzzle.jpg.pagespeed.a.ic.fakehash.jpg"
IMG_B="$EXAMPLE/images/xPuzzle.jpg.pagespeed.b.ic.fakehash.jpg"
fetch_until $IMG_A 'wc -c' 102902 "" -le
fetch_until $IMG_B 'wc -c'  98276 "" -le
