# Test that we can make options sticky using cookies.

start_test Sticky option cookies: initially remove_comments only
URL="$(generate_url options-by-cookies-enabled.example.com \
                    /mod_pagespeed_test/forbidden.html)"
COOKIES=""
PARAMS=""
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_from     "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_not_from "$OUT" grep -q 'Cookie'

start_test Sticky option cookies: wrong token has no effect
PARAMS="?PageSpeedStickyQueryParameters=wrong_secret"
PARAMS+="&PageSpeedFilters=+remove_comments"
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_not_from "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_not_from "$OUT" grep -q 'Set-Cookie'

start_test Sticky option cookies: right token IS adhesive
PARAMS="?PageSpeedStickyQueryParameters=sticky_secret"
PARAMS+="&PageSpeedFilters=+remove_comments"
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_not_from "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_from "$OUT" grep -q 'Set-Cookie: PageSpeedFilters=%2bremove_comments;'
# We know we got the right cookie, now check that we got the right number.
check [ $(echo "$OUT" | egrep -c 'Set-Cookie:') = 1 ]

start_test Sticky option cookies: no token leaves option cookies untouched
COOKIES=$(echo "$OUT" | extract_cookies)
PARAMS=""
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_not_from "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_not_from "$OUT" grep -q 'Set-Cookie'

start_test Sticky option cookies: wrong token expires option cookies
PARAMS="?PageSpeedStickyQueryParameters=off"
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_not_from "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_from "$OUT" grep -q 'Cookie: PageSpeedFilters; Expires=Thu, 01 Jan 1970'
COOKIES=$(echo "$OUT" | grep -v 'Expires=Thu, 01 Jan 1970'| extract_cookies)
check [ -z "$COOKIES" ]

start_test Sticky option cookies: back to remove_comments only
PARAMS=""
echo wget $COOKIES $URL$PARAMS
OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $COOKIES $URL$PARAMS)"
check_from     "$OUT" grep -q '<!-- This comment should not be deleted -->'
check_not_from "$OUT" grep -q '  '
check_not_from "$OUT" grep -q 'Cookie'
