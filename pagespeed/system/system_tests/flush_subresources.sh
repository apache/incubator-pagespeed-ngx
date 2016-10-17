start_test flush_subresources rewriter is not applied
URL="$TEST_ROOT/flush_subresources.html?\
PageSpeedFilters=flush_subresources,extend_cache_css,\
extend_cache_scripts"
# Fetch once with X-PSA-Blocking-Rewrite so that the resources get rewritten and
# property cache is updated with them.
wget -O - --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $TESTTMP/flush
# Fetch again. The property cache has the subresources this time but
# flush_subresources rewriter is not applied. This is a negative test case
# because this rewriter does not exist in pagespeed yet.
check [ `wget -O - $URL | grep -o 'link rel="subresource"' | wc -l` = 0 ]
rm -f $TESTTMP/flush
