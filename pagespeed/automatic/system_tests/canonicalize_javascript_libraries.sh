# Checks that we can correctly identify a known library url.
test_filter canonicalize_javascript_libraries finds library urls
fetch_until $URL 'fgrep -c http://www.modpagespeed.com/rewrite_javascript.js' 1
