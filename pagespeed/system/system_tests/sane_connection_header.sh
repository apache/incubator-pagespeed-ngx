# Test to make sure we have a sane Connection Header.  See
# https://github.com/pagespeed/mod_pagespeed/issues/664
#
# Note that this bug is dependent on seeing a resource for the first time in
# the InPlaceResourceOptimization path, because in that flow we are caching
# the response-headers from the server.  The reponse-headers from Serf never
# seem to include the Connection header.  So we have to cachebust the JS file.
start_test Sane Connection header
URL="$TEST_ROOT/normal.js?q=cachebust"
fetch_until -save $URL 'grep -c W/\"PSA-aj-' 1 --save-headers
CONNECTION=$(extract_headers $FETCH_UNTIL_OUTFILE | fgrep "Connection:")
check_not_from "$CONNECTION" fgrep -qi "Keep-Alive, Keep-Alive"
check_from "$CONNECTION" fgrep -qi "Keep-Alive"
