start_test load from file with ipro
URL="http://lff-ipro.example.com/mod_pagespeed_test/lff_ipro/fake.woff"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - $URL)
check_from "$OUT" grep "^This isn't really a woff file\.$"
check [ "$(echo "$OUT" | wc -l)" = 1 ]
