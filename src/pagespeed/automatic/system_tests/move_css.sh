start_test move_css_above_scripts works.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=move_css_above_scripts
FETCHED=$OUTDIR/output-file
$WGET_DUMP $URL > $FETCHED
# Link moved before script.
check grep -q "styles/all_styles.css\"><script" $FETCHED

start_test move_css_above_scripts off.
URL=$EXAMPLE_ROOT/move_css_above_scripts.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved before script.
check_not grep "styles/all_styles.css\"><script" $FETCHED

start_test move_css_to_head does what it says on the tin.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=move_css_to_head
$WGET_DUMP $URL > $FETCHED
# Link moved to head.
check grep -q "styles/all_styles.css\"></head>" $FETCHED

start_test move_css_to_head off.
URL=$EXAMPLE_ROOT/move_css_to_head.html?PageSpeedFilters=
$WGET_DUMP $URL > $FETCHED
# Link not moved to head.
check_not grep "styles/all_styles.css\"></head>" $FETCHED
