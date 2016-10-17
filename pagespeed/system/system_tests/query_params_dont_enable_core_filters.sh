start_test query params dont turn on core filters
# See https://github.com/pagespeed/ngx_pagespeed/issues/1190
URL="debug-filters.example.com/mod_pagespeed_example/"
URL+="rewrite_javascript.html?PageSpeedFilters=-rewrite_css"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
FILTERS=$(extract_filters_from_debug_html "$OUT")
check_from "$FILTERS" grep -q "^db.*Debug$"
check_from "$FILTERS" grep -q "^hw.*Flushes html$"
check_not_from "$FILTERS" grep -q "^jm.*Rewrite External Javascript$"
check_not_from "$FILTERS" grep -q "^jj.*Rewrite Inline Javascript$"
