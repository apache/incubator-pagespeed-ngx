start_test IPRO source map tests
URL="$TEST_ROOT/experimental_js_minifier/script.js"
URL+="?PageSpeedFilters=rewrite_javascript,include_js_source_maps"
# Fetch until IPRO removes comments.
fetch_until -save $URL 'grep -c removed' 0
# Yes source_map for external JS
check grep -q sourceMappingURL $FETCH_FILE
SOURCE_MAP_URL=$(grep sourceMappingURL $FETCH_FILE | grep -o 'http://.*')
OUTFILE=$OUTDIR/source_map
check $WGET_DUMP -O $OUTFILE $SOURCE_MAP_URL
check grep -qi "Cache-control: max-age=31536000" $OUTFILE  # Long cache
check grep -q "script.js?PageSpeed=off" $OUTFILE  # Has source URL.
check grep -q '"mappings":' $OUTFILE  # Has mappings.
