start_test mod_rewrite
check $WGET_DUMP $TEST_ROOT/redirect/php/ -O $OUTDIR/redirect_php.html
check \
  [ $(grep -ce "href=\"/mod_pagespeed_test/" $OUTDIR/redirect_php.html) = 2 ];
