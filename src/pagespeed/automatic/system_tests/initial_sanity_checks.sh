# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.
start_test directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q $EXAMPLE_ROOT/?PageSpeed=off -O $OUTDIR/mod_pagespeed_example
check $WGET -q $EXAMPLE_ROOT/index.html?PageSpeed=off -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example

start_test compression is enabled for HTML.
OUT=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' $EXAMPLE_ROOT/ 2>&1)
check_from "$OUT" fgrep -qi 'Content-Encoding: gzip'

start_test We behave sanely on whitespace served as HTML
OUT=$($WGET_DUMP $TEST_ROOT/whitespace.html)
check_200_http_response "$OUT"
