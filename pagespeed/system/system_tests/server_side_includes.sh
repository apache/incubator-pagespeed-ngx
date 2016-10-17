start_test server-side includes
fetch_until -save $TEST_ROOT/ssi/ssi.shtml?PageSpeedFilters=combine_css \
    'fgrep -c .pagespeed.' 1
check [ $(grep -ce $combine_css_filename $FETCH_FILE) = 1 ];
