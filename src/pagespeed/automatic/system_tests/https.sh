start_test Simple test that https is working.
if [ -n "$HTTPS_HOST" ]; then
  URL="$HTTPS_EXAMPLE_ROOT/combine_css.html"
  fetch_until $URL 'fgrep -c css+' 1 --no-check-certificate

  start_test https is working.
  echo $WGET_DUMP_HTTPS $URL
  HTML_HEADERS=$($WGET_DUMP_HTTPS $URL)

  echo Checking for X-Mod-Pagespeed header
  check_from "$HTML_HEADERS" egrep -q 'X-Mod-Pagespeed|X-Page-Speed'

  echo Checking for combined CSS URL
  EXPECTED='href="styles/yellow\.css+blue\.css+big\.css+bold\.css'
  EXPECTED="$EXPECTED"'\.pagespeed\.cc\..*\.css"/>'
  fetch_until "$URL?PageSpeedFilters=combine_css,trim_urls" \
      "grep -ic $EXPECTED" 1 --no-check-certificate

  echo Checking for combined CSS URL without URL trimming
  # Without URL trimming we still preserve URL relativity.
  fetch_until "$URL?PageSpeedFilters=combine_css" "grep -ic $EXPECTED" 1 \
     --no-check-certificate
fi
