start_test IPRO requests are routed through the controller API

STATS=$OUTDIR/controller_stats
$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.0

OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css?random=$RANDOM)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

$WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.1
check_stat $STATS.0 $STATS.1 named-lock-rewrite-scheduler-granted 0
check_stat $STATS.0 $STATS.1 popularity-contest-num-rewrites-succeeded 1
