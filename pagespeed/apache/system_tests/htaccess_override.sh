start_test Make sure Disallow/Allow overrides work in htaccess hierarchies
DISALLOWED=$($WGET_DUMP "$TEST_ROOT"/htaccess/purple.css)
check_from "$DISALLOWED" fgrep -q MediumPurple
fetch_until "$TEST_ROOT"/htaccess/override/purple.css \
    'fgrep -c background:#9370db' 1
