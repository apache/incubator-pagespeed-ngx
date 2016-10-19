start_test Fetch gzipped, make sure that we have cache compressed at gzip 9.
URL="$PRIMARY_SERVER/mod_pagespeed_test/invalid.css"
fetch_until -gzip $URL "wc -c" 27
