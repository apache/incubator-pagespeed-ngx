  start_test add_instrumentation has added unload handler with \
    ModPagespeedReportUnloadTime enabled in APACHE_SECONDARY_PORT.
URL="$SECONDARY_TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation"
echo http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
http_proxy=$SECONDARY_HOSTNAME $WGET -O $WGET_OUTPUT $URL
check [ $(grep -o "<script" $WGET_OUTPUT|wc -l) = 3 ]
check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'beforeunload', '', '$SECONDARY_TEST_ROOT/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
check [ $(grep -c "pagespeed.addInstrumentationInit('/$BEACON_HANDLER', 'load', '', '$SECONDARY_TEST_ROOT/add_instrumentation.html');" $WGET_OUTPUT) = 1 ]
