start_test can combine css with authorized ids only
URL="$TEST_ROOT/combine_css_with_ids.html?PageSpeedFilters=combine_css"
# Test big.css and bold.css are combined, but not yellow.css or blue.css.
fetch_until -save "$URL" 'fgrep -c styles/big.css+bold.css.pagespeed.cc' 1
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/yellow.css" id='
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/blue.css" id='
