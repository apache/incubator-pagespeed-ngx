start_test Accept bad query params and headers

# The examples page should have this EXPECTED_EXAMPLES_TEXT on it.
EXPECTED_EXAMPLES_TEXT="PageSpeed Examples Directory"
OUT=$(wget -O - $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# It should still be there with bad query params.
OUT=$(wget -O - $EXAMPLE_ROOT?PageSpeedFilters=bogus)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# And also with bad request headers.
OUT=$(wget -O - --header=PageSpeedFilters:bogus $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"
