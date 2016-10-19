# This is dependent upon having a beacon handler.
test_filter add_instrumentation beacons load.
OUT=$($CURL -sSi $PRIMARY_SERVER/$BEACON_HANDLER?ets=load:13)
check_from "$OUT" fgrep -q "204 No Content"
check_from "$OUT" fgrep -q 'Cache-Control: max-age=0, no-cache'
