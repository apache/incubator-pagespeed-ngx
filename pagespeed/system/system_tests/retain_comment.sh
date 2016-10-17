# Test RetainComment directive.
test_filter remove_comments retains appropriate comments.
check run_wget_with_args $URL
check fgrep -q retained $FETCHED        # RetainComment directive
