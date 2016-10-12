if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
  start_test Follow flushes does what it should do.
  echo "Check that FollowFlushes on outputs timely chunks"
  # The php file will write 6 chunks, but the last two often get aggregated
  # into one. Hence 5 or 6 is what we want to see.
  check_flushing flush 2.2 5
fi

