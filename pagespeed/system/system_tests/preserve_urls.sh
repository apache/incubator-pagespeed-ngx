# Make sure that when in PreserveURLs mode that we don't rewrite URLs. This is
# non-exhaustive, the unit tests should cover the rest.  Note: We block with
# psatest here because this is a negative test.  We wouldn't otherwise know
# how many wget attempts should be made.
start_test PreserveURLs on prevents URL rewriting
WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
WGET_ARGS+=" --header=Host:preserveurls.example.com"

FILE=preserveurls/on/preserveurls.html
URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
FETCHED=$OUTDIR/preserveurls.html
check run_wget_with_args $URL
check_not fgrep -q .pagespeed. $FETCHED

# When PreserveURLs is off do a quick check to make sure that normal rewriting
# occurs.  This is not exhaustive, the unit tests should cover the rest.
start_test PreserveURLs off causes URL rewriting
WGET_ARGS="--header=Host:preserveurls.example.com"
FILE=preserveurls/off/preserveurls.html
URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
FETCHED=$OUTDIR/preserveurls.html
# Check that style.css was inlined.
fetch_until $URL 'egrep -c big.css.pagespeed.' 1
# Check that introspection.js was inlined.
fetch_until $URL 'grep -c document\.write(\"External' 1
# Check that the image was optimized.
fetch_until $URL 'grep -c BikeCrashIcn\.png\.pagespeed\.' 1
