start_test When ModPagespeedMaxHtmlParseBytes is not set, we do not insert \
           a redirect.
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
  $SECONDARY_TEST_ROOT/large_file.html?PageSpeedFilters=)
check_not_from "$OUT" fgrep -q "window.location="
check_from "$OUT" fgrep -q "Lorem ipsum dolor sit amet"
