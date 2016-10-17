start_test 404s are served and properly recorded.
echo $STATISTICS_URL
NUM_404=$(scrape_stat resource_404_count)
echo "Initial 404s: $NUM_404"
WGET_ERROR=$(check_not $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
check_from "$WGET_ERROR" fgrep -q "404 Not Found"

# Check that the stat got bumped.
NUM_404_FINAL=$(scrape_stat resource_404_count)
echo "Final 404s: $NUM_404_FINAL"
check [ $(expr $NUM_404_FINAL - $NUM_404) -eq 1 ]

# Check that the stat doesn't get bumped on non-404s.
URL="$PRIMARY_SERVER/mod_pagespeed_example/styles/"
URL+="W.rewrite_css_images.css.pagespeed.cf.Hash.css"
OUT=$(wget -O - -q $URL)
check_from "$OUT" grep background-image
NUM_404_REALLY_FINAL=$(scrape_stat resource_404_count)
check [ $NUM_404_FINAL -eq $NUM_404_REALLY_FINAL ]
