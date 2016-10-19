start_test Send custom fetch headers on resource re-fetches.
PLAIN_HEADER="header=value"
X_OTHER_HEADER="x-other=False"

URL="$PRIMARY_SERVER/mod_pagespeed_log_request_headers.js.pagespeed.jm.0.js"
WGET_OUT=$($WGET_DUMP $URL)
check_from "$WGET_OUT" grep "$PLAIN_HEADER"
check_from "$WGET_OUT" grep "$X_OTHER_HEADER"

start_test Send custom fetch headers on resource subfetches.
URL=$TEST_ROOT/custom_fetch_headers.html?PageSpeedFilters=inline_javascript
fetch_until -save $URL 'grep -c header=value' 1
check_from "$(cat $FETCH_FILE)" grep "$X_OTHER_HEADER"
