start_test respect vary user-agent
URL="$SECONDARY_HOSTNAME/mod_pagespeed_test/vary/index.html"
URL+="?PageSpeedFilters=inline_css"
FETCH_CMD="$WGET_DUMP --header=Host:respectvary.example.com $URL"
OUT=$($FETCH_CMD)
# We want to verify that css is not inlined, but if we just check once then
# pagespeed doesn't have long enough to be able to inline it.
sleep .1
OUT=$($FETCH_CMD)
check_not_from "$OUT" fgrep "<style>"
