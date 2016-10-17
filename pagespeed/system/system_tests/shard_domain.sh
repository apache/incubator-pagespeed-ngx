start_test ShardDomain directive in per-directory config
fetch_until -save $TEST_ROOT/shard/shard.html 'fgrep -c .pagespeed.ce' 4
check [ $(grep -ce href=\"http://shard1 $FETCH_FILE) = 2 ];
check [ $(grep -ce href=\"http://shard2 $FETCH_FILE) = 2 ];
