# Checks that local_storage_cache injects optimized javascript from
# local_storage_cache.js, adds the pagespeed_lsc_ attributes, inlines the data
# (if the cache were empty the inlining wouldn't make the timer cutoff but the
# resources have been fetched above).
test_filter local_storage_cache,inline_css,inline_images optimize mode
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args "$URL"
check run_wget_with_args "$URL"
check grep -q "pagespeed.localStorageCacheInit()" $FETCHED
check [ $(grep -c ' pagespeed_lsc_url=' $FETCHED) = 2 ]
check grep -q "yellow {background-color: yellow" $FETCHED
check grep -q "<img src=\"data:image/png;base64" $FETCHED
check grep -q "<img .* alt=\"A cup of joe\"" $FETCHED
check_not grep -q "/\*" $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that local_storage_cache,debug injects debug javascript from
# local_storage_cache.js, adds the pagespeed_lsc_ attributes, inlines the data
# (if the cache were empty the inlining wouldn't make the timer cutoff but the
# resources have been fetched above).
test_filter local_storage_cache,inline_css,inline_images,debug debug mode
# Force the request to be rewritten with all applicable filters.
WGET_ARGS="${WGET_ARGS} --header=X-PSA-Blocking-Rewrite:psatest"
echo run_wget_with_args "$URL"
check run_wget_with_args "$URL"
check grep -q "pagespeed.localStorageCacheInit()" $FETCHED
check [ $(grep -c ' pagespeed_lsc_url=' $FETCHED) = 2 ]
check grep -q "yellow {background-color: yellow" $FETCHED
check grep -q "<img src=\"data:image/png;base64" $FETCHED
check grep -q "<img .* alt=\"A cup of joe\"" $FETCHED
check_not grep -q "/\*" $FETCHED
check_not grep -q "goog.require" $FETCHED
check grep -q "PageSpeed=noscript" $FETCHED

# Checks that local_storage_cache doesn't send the inlined data for a resource
# whose hash is in the magic cookie. First get the cookies from prior runs.
HASHES=$(grep "pagespeed_lsc_hash=" $FETCHED |\
         sed -e 's/^.*pagespeed_lsc_hash=.//' |\
         sed -e 's/".*$//')
HASHES=$(echo "$HASHES" | tr '\n' '!' | sed -e 's/!$//')
check [ -n "$HASHES" ]
COOKIE="Cookie: _GPSLSC=$HASHES"
# Check that the prior run did inline the data.
check grep -q "background-color: yellow" $FETCHED
check grep -q "src=.data:image/png;base64," $FETCHED
check grep -q "alt=.A cup of joe." $FETCHED
# Fetch with the cookie set.
test_filter local_storage_cache,inline_css,inline_images cookies set
check run_wget_with_args --save-headers --no-cookies --header "$COOKIE" $URL
# Check that this run did NOT inline the data.
check_not fgrep "yellow {background-color: yellow" $FETCHED
check_not grep "src=.data:image/png;base64," $FETCHED
# Check that this run inserted the expected scripts.
check grep -q \
  "pagespeed.localStorageCache.inlineCss(.http://.*/styles/yellow.css.);" \
  $FETCHED

SEARCH_FOR="pagespeed.localStorageCache.inlineImg("
SEARCH_FOR+=".http://.*/images/Cuppa.png., [^,]*, "
SEARCH_FOR+=".alt=A cup of joe., "
SEARCH_FOR+=".alt=A cup of joe., "
SEARCH_FOR+=".alt=A cup of joe..s ..joe..., "
SEARCH_FOR+=".alt=A cup of joe..s ..joe...);"
check grep -q "$SEARCH_FOR" $FETCHED
