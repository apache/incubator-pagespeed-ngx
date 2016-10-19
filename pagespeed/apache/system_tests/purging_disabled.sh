start_test Base config has purging disabled.  Check error message syntax.
OUT=$($WGET_DUMP "$HOSTNAME/pagespeed_admin/cache?purge=*")
check_from "$OUT" fgrep -q "ModPagespeedEnableCachePurge on"
