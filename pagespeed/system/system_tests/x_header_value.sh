# This test checks that the XHeaderValue directive works.
start_test XHeaderValue directive

RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
  http://xheader.example.com/mod_pagespeed_example)
check_from "$RESPONSE_OUT" \
  egrep -q "X-(Page-Speed|Mod-Pagespeed): UNSPECIFIED VERSION"
