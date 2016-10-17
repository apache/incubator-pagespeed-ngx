# This test only makes sense if you're running tests against localhost.
if echo "$HOSTNAME" | grep "^localhost:"; then
  if which ifconfig >/dev/null; then
    start_test Non-local access to statistics fails.
    NON_LOCAL_IP=$( \
      ifconfig | egrep -o 'inet addr:[0-9]+.[0-9]+.[0-9]+.[0-9]+' | \
      awk -F: '{print $2}' | grep -v ^127 | head -n 1)

    # Make sure pagespeed is listening on NON_LOCAL_IP.
    URL="http://$NON_LOCAL_IP:$(echo $HOSTNAME | sed s/^localhost://)/"
    URL+="mod_pagespeed_example/styles/"
    URL+="W.rewrite_css_images.css.pagespeed.cf.Hash.css"
    OUT=$($CURL -Ssi $URL)
    check_from "$OUT" grep background-image

    # Make sure we can't load statistics from NON_LOCAL_IP.
    ALT_STAT_URL=$(echo $STATISTICS_URL | sed s#localhost#$NON_LOCAL_IP#)

    echo "wget $ALT_STAT_URL >& $TESTTMP/alt_stat_url"
    check_error_code 8 wget $ALT_STAT_URL >& "$TESTTMP/alt_stat_url"
    rm -f "$TESTTMP/alt_stat_url"

    ALT_CE_URL="$ALT_STAT_URL.pagespeed.ce.8CfGBvwDhH.css"
    check_error_code 8 wget -O - $ALT_CE_URL  >& "$TESTTMP/alt_ce_url"
    check_error_code 8 wget -O - --header="Host: $HOSTNAME" $ALT_CE_URL \
      >& "$TESTTMP/alt_ce_url"
    rm -f "$TESTTMP/alt_ce_url"
  fi
fi
