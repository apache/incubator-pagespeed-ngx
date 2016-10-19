# Note: There is a similar test in system_test.sh
# Test /mod_pagespeed_message exists.
start_test Check if /mod_pagespeed_message page exists.
OUT=$($WGET --save-headers -q -O - $MESSAGE_URL | head -1)
check_200_http_response "$OUT"
