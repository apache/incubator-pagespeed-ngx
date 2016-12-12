#!/bin/bash
#
# Copyright 2013 Google Inc.
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
if [ "${FIRST_RUN:-}" = "true" ]; then
  # Likewise, blocking rewrite tests are only run once.
  start_test Blocking rewrite enabled.
  # We assume that blocking_rewrite_test_dont_reuse_1.jpg will not be
  # rewritten on the first request since it takes significantly more time to
  # rewrite than the rewrite deadline and it is not already accessed by
  # another request earlier.
  BLOCKING_REWRITE_URL="$TEST_ROOT/blocking_rewrite.html?\
PageSpeedFilters=rewrite_images"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  OLDSTATS=$OUTDIR/blocking_rewrite_stats.old
  NEWSTATS=$OUTDIR/blocking_rewrite_stats.new
  $WGET_DUMP $STATISTICS_URL > $OLDSTATS
  check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest'\
    $BLOCKING_REWRITE_URL -O $OUTFILE
  $WGET_DUMP $STATISTICS_URL > $NEWSTATS
  check_stat $OLDSTATS $NEWSTATS image_rewrites 1
  check_stat $OLDSTATS $NEWSTATS cache_hits 0
  check_stat $OLDSTATS $NEWSTATS cache_misses 2
  # 2 cache inserts for image.  There should not be an ipro-related
  # cache-write for the HTML because we bail early on non-ipro-rewritable
  # content-types.
  check_stat $OLDSTATS $NEWSTATS cache_inserts 2
  # TODO(sligocki): There is no stat num_rewrites_executed. Fix.
  #check_stat $OLDSTATS $NEWSTATS num_rewrites_executed 1

  start_test Blocking rewrite enabled using wrong key.
  BLOCKING_REWRITE_URL="$SECONDARY_TEST_ROOT/\
blocking_rewrite_another.html?PageSpeedFilters=rewrite_images"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: junk' \
    $BLOCKING_REWRITE_URL > $OUTFILE
  check [ $(grep -c "[.]pagespeed[.]" $OUTFILE) -lt 1 ]

  http_proxy=$SECONDARY_HOSTNAME fetch_until $BLOCKING_REWRITE_URL 'grep -c [.]pagespeed[.]' 1
fi
