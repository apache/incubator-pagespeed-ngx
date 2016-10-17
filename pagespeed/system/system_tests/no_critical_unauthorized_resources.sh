test_filter prioritize_critical_css

start_test no critical selectors chosen from unauthorized resources
URL="$TEST_ROOT/unauthorized/prioritize_critical_css.html"
URL+="?PageSpeedFilters=prioritize_critical_css,debug"
fetch_until -save $URL 'fgrep -c pagespeed.criticalCssBeaconInit' 3
# Except for the occurrence in html, the gsc-completion-selected string
# should not occur anywhere else, i.e. in the selector list.
check [ $(fgrep -c "gsc-completion-selected" $FETCH_FILE) -eq 1 ]
# From the css file containing an unauthorized @import line,
# a) no selectors from the unauthorized @ import (e.g .maia-display) should
#    appear in the selector list.
check_not fgrep -q "maia-display" $FETCH_FILE
# b) no selectors from the authorized @ import (e.g .interesting_color) should
#    appear in the selector list because it won't be flattened.
check_not fgrep -q "interesting_color" $FETCH_FILE
# c) selectors that don't depend on flattening should appear in the selector
#    list.
check [ $(fgrep -c "non_flattened_selector" $FETCH_FILE) -eq 1 ]
EXPECTED_IMPORT_FAILURE_LINE="<!--Flattening failed: Cannot import http://www.google.com/css/maia.css as it is on an unauthorized domain-->"
check [ $(grep -o "$EXPECTED_IMPORT_FAILURE_LINE" $FETCH_FILE | wc -l) -eq 1 ]
EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because its domain (cse.google.com) is not authorized-->"
check [ $(grep -o "$EXPECTED_COMMENT_LINE" $FETCH_FILE | wc -l) -eq 1 ]

start_test inline_unauthorized_resources allows unauthorized css selectors
HOST_NAME="http://unauthorizedresources.example.com"
URL="$HOST_NAME/mod_pagespeed_test/unauthorized/prioritize_critical_css.html"
URL+="?PageSpeedFilters=prioritize_critical_css,debug"
# gsc-completion-selected string should occur once in the html and once in the
# selector list.  with_unauthorized_imports.css.pagespeed.cf should appear
# once that file has been optimized.  We need to wait until both of them have
# been optimized.
REWRITTEN_UNAUTH_CSS="with_unauthorized_imports\.css\.pagespeed\.cf"
GSC_SELECTOR="gsc-completion-selected"
function unauthorized_resources_fully_rewritten() {
  tr '\n' ' ' | \
    grep "$REWRITTEN_UNAUTH_CSS.*$GSC_SELECTOR.*$GSC_SELECTOR" | \
    wc -l
}
http_proxy=$SECONDARY_HOSTNAME \
   fetch_until -save $URL unauthorized_resources_fully_rewritten 1
# Verify that this page had beaconing javascript on it.
check [ $(fgrep -c "pagespeed.criticalCssBeaconInit" $FETCH_FILE) -eq 3 ]
# From the css file containing an unauthorized @import line,
# a) no selectors from the unauthorized @ import (e.g .maia-display) should
#    appear in the selector list.
check_not fgrep -q "maia-display" $FETCH_FILE
# b) no selectors from the authorized @ import (e.g .red) should
#    appear in the selector list because it won't be flattened.
check_not fgrep -q "interesting_color" $FETCH_FILE
# c) selectors that don't depend on flattening should appear in the selector
#    list.
check [ $(fgrep -c "non_flattened_selector" $FETCH_FILE) -eq 1 ]
check_from "$(cat $FETCH_FILE)" grep -q "$EXPECTED_IMPORT_FAILURE_LINE"

