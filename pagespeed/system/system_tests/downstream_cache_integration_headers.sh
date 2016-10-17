start_test Downstream cache integration caching headers.
URL="http://downstreamcacheresource.example.com/mod_pagespeed_example/images/"
URL+="xCuppa.png.pagespeed.ic.0.png"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
check_from "$OUT" egrep -iq $'^Cache-Control: .*\r$'
check_from "$OUT" egrep -iq $'^Expires: .*\r$'
check_from "$OUT" egrep -iq $'^Last-Modified: .*\r$'
