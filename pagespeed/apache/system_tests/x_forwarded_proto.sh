start_test Respect X-Forwarded-Proto when told to
FETCHED=$OUTDIR/x_forwarded_proto
URL=$TEST_ROOT/?PageSpeedFilters=add_base_tag
check $WGET_DUMP -O $FETCHED --header="X-Forwarded-Proto: https" $URL
# When enabled, we respect X-Forwarded-Proto and thus list base as https.
check fgrep -q '<base href="https://' $FETCHED
