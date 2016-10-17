start_test cache_partial_html enabled has no effect
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=cache_partial_html
check [ $(grep -c '<html>' $WGET_OUTPUT) = 1 ]
check [ $(grep -c '<body>' $WGET_OUTPUT) = 1 ]
check [ $(grep -c 'pagespeed.panelLoader' $WGET_OUTPUT) = 0 ]

