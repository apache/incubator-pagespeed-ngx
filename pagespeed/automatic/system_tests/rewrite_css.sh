test_filter rewrite_css minifies CSS and saves bytes.
fetch_until -save $URL 'grep -c comment' 0
check_file_size $FETCH_FILE -lt 680  # down from 689
