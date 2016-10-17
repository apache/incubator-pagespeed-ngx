start_test long url handling
# This is an extremely long url, enough that it should give a 4xx server error.
OUT=$($CURL -sS -D- "$TEST_ROOT/$(head -c 10000 < /dev/zero | tr '\0' 'a')")
check_from "$OUT" grep -q "414 Request-URI Too Large\|414 Request-URI Too Long"
