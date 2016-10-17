test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'grep -c text/css' 1
check run_wget_with_args $URL
test_resource_ext_corruption $URL $combine_css_filename

test_filter extend_cache rewrites an image tag.
fetch_until $URL 'grep -c src.*91_WewrLtP' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg
