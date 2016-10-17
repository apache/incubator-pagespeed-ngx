start_test authorized resources do not get cached and optimized.
URL="$TEST_ROOT/auth/medium_purple.css"
AUTH="Authorization:Basic dXNlcjE6cGFzc3dvcmQ="
not_cacheable_start=$(scrape_stat ipro_recorder_not_cacheable)
echo $WGET_DUMP --header="$AUTH" "$URL"
OUT=$($WGET_DUMP --header="$AUTH" "$URL")
check_from "$OUT" fgrep -q 'background: MediumPurple;'
not_cacheable=$(scrape_stat ipro_recorder_not_cacheable)
check [ $not_cacheable = $((not_cacheable_start + 1)) ]
URL=""
AUTH=""
