# Test lying host headers for cross-site fetch. This should fetch from localhost
# and therefore succeed.
start_test Lying host headers for cross-site fetch
EVIL_URL=$HOSTNAME/mod_pagespeed_example/styles/big.css.pagespeed.ce.8CfGBvwDhH.css
echo wget --save-headers -O - '--header=Host:www.google.com' $EVIL_URL
check wget --save-headers -O - '--header=Host:www.google.com' $EVIL_URL >& $TESTTMP/evil
rm -f $TESTTMP/evil
