test_filter elide_attributes removes boolean and default attributes.
check run_wget_with_args $URL
check_not fgrep "disabled=" $FETCHED   # boolean, should not find
