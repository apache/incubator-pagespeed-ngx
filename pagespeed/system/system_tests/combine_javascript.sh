test_filter combine_javascript combines 2 JS files into 1.
start_test combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?PageSpeedFilters=combine_javascript
fetch_until $URL 'grep -c src=' 4

start_test aris disables js combining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/"
URL+="?PageSpeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 2

start_test aris disables js combining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html"
URL+="?PageSpeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 1
