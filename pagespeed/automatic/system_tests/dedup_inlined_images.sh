# Test dedup_inlined_images
test_filter dedup_inlined_images,inline_images
fetch_until -save $URL 'fgrep -ocw inlineImg(' 4
check grep -q "PageSpeed=noscript" $FETCH_FILE
