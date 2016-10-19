# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.
start_test directory is mapped to index.html.
rm -rf $OUTDIR
mkdir -p $OUTDIR
check $WGET -q "$EXAMPLE_ROOT/?PageSpeed=off" \
  -O $OUTDIR/mod_pagespeed_example
check $WGET -q "$EXAMPLE_ROOT/index.html?PageSpeed=off" -O $OUTDIR/index.html
check diff $OUTDIR/index.html $OUTDIR/mod_pagespeed_example
