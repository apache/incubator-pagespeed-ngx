start_test Filters do not rewrite blacklisted JavaScript files.
URL=$TEST_ROOT/blacklist/blacklist.html?PageSpeedFilters=extend_cache,rewrite_javascript,trim_urls
fetch_until -save $URL 'grep -c .js.pagespeed.' 4
FETCHED=$FETCH_UNTIL_OUTFILE
check grep -q "<script src=\".*normal\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\"js_tinyMCE\.js\"></script>" $FETCHED
check grep -q "<script src=\"tiny_mce\.js\"></script>" $FETCHED
check grep -q "<script src=\"tinymce\.js\"></script>" $FETCHED
check grep -q \
  "<script src=\"scriptaculous\.js?load=effects,builder\"></script>" $FETCHED
check grep -q "<script src=\".*jquery.*\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q "<script src=\".*ckeditor\.js\">" $FETCHED
check grep -q "<script src=\".*swfobject\.js\.pagespeed\..*\.js\">" $FETCHED
check grep -q \
  "<script src=\".*another_normal\.js\.pagespeed\..*\.js\">" $FETCHED
