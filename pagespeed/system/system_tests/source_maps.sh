start_test UseExperimentalJsMinifier
URL="$TEST_ROOT/experimental_js_minifier/index.html"
URL+="?PageSpeedFilters=rewrite_javascript"
# External scripts rewritten.
fetch_until -save -recursive $URL 'grep -c src=.*\.pagespeed\.jm\.' 1
check_not grep "removed" $WGET_DIR/*   # No comments should remain.
check grep -q "preserved" $WGET_DIR/*  # Contents of <script src=> element kept.
ORIGINAL_HTML_SIZE=1484
check_file_size $FETCH_FILE -lt $ORIGINAL_HTML_SIZE  # Net savings
# Rewritten JS is cache-extended.
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

start_test Source map tests
URL="$TEST_ROOT/experimental_js_minifier/index.html"
URL+="?PageSpeedFilters=rewrite_javascript,include_js_source_maps"
# All rewriting still happening as expected.
fetch_until -save -recursive $URL 'grep -c src=.*\.pagespeed\.jm\.' 1
check_not grep "removed" $WGET_DIR/*  # No comments should remain.
check_file_size $FETCH_FILE -lt $ORIGINAL_HTML_SIZE  # Net savings
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

# No source map for inline JS
check_not grep sourceMappingURL $FETCH_FILE
# Yes source_map for external JS
check grep -q sourceMappingURL $WGET_DIR/script.js.pagespeed.*
SOURCE_MAP_URL=$(grep sourceMappingURL $WGET_DIR/script.js.pagespeed.* |
                 grep -o 'http://.*')
OUTFILE=$OUTDIR/source_map
check $WGET_DUMP -O $OUTFILE $SOURCE_MAP_URL
check grep -qi "Cache-control: max-age=31536000" $OUTFILE  # Long cache
check grep -q "script.js?PageSpeed=off" $OUTFILE  # Has source URL.
check grep -q '"mappings":' $OUTFILE  # Has mappings.
