start_test HTML add_instrumentation CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
# In some servers PageSpeed runs before response headers are finalized, which
# means it has to  assume the page is xhtml because the 'Content-Type' header
# might just not have been set yet.  In others it runs after, and so it can
# trust what it sees in the headers.  See RewriteDriver::MimeTypeXhtmlStatus().
if $HEADERS_FINALIZED; then
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 0 ]
else
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]
fi

start_test XHTML add_instrumentation also lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.xhtml\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]
