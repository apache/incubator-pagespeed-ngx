if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
  start_test Follow flushes does what it should do.
  echo "Check that FollowFlushes on outputs timely chunks"
  URL="http://flush.example.com/mod_pagespeed_test/slow_flushing_html_response.php"
  # The php file will write 6 chunks, but the last two often get aggregated
  # into one. Hence 5 or 6 is what we want to see.
  check_flushing "$CURL -N --raw --silent --proxy $SECONDARY_HOSTNAME $URL" \
    2.2 5
fi

