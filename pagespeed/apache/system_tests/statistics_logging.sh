start_test Statistics logging works.
check ls $MOD_PAGESPEED_STATS_LOG
check [ $(grep "timestamp: " $MOD_PAGESPEED_STATS_LOG | wc -l) -ge 1 ]
# An array of all the timestamps that were logged.
TIMESTAMPS=($(sed -n '/timestamp: /s/[^0-9]*//gp' $MOD_PAGESPEED_STATS_LOG))
check [ ${#TIMESTAMPS[@]} -ge 1 ]
for T in ${TIMESTAMPS[@]}; do
  check [ $T -ge $START_TIME ]
done
# Check a few arbitrary statistics to make sure logging is taking place.
check [ $(grep "num_flushes: " $MOD_PAGESPEED_STATS_LOG | wc -l) -ge 1 ]
# We are not outputting histograms.
check [ $(grep "histogram#" $MOD_PAGESPEED_STATS_LOG | wc -l) -eq 0 ]
check [ $(grep "image_ongoing_rewrites: " $MOD_PAGESPEED_STATS_LOG | wc -l) \
  -ge 1 ]

start_test Statistics logging JSON handler works.
JSON=$OUTDIR/console_json.json
STATS_JSON_URL="$CONSOLE_URL?json&granularity=0&var_titles=num_\
flushes,image_ongoing_rewrites"
echo "$WGET_DUMP $STATS_JSON_URL > $JSON"
$WGET_DUMP $STATS_JSON_URL > $JSON
# Each variable we ask for should show up once.
check [ $(grep "\"num_flushes\": " $JSON | wc -l) -eq 1 ]
check [ $(grep "\"image_ongoing_rewrites\": " $JSON | wc -l) -eq 1 ]
check [ $(grep "\"timestamps\": " $JSON | wc -l) -eq 1 ]
# An array of all the timestamps that the JSON handler returned.
JSON_TIMESTAMPS=($(sed -rn 's/^\{"timestamps": \[(([0-9]+, )*[0-9]*)\].*}$/\1/;/^[0-9]+/s/,//gp' $JSON))
# Check that we see the same timestamps that are in TIMESTAMPS.
# We might have generated extra timestamps in the time between TIMESTAMPS
# and JSON_TIMESTAMPS, so only loop through TIMESTAMPS.
check [ ${#JSON_TIMESTAMPS[@]} -ge ${#TIMESTAMPS[@]} ]
t=0
while [ $t -lt ${#TIMESTAMPS[@]} ]; do
  check [ ${TIMESTAMPS[$t]} -eq ${JSON_TIMESTAMPS[$t]} ]
  t=$(($t+1))
done

start_test JSON handler does not mirror HTML
JSON_URL="$CONSOLE_URL?json&granularity=0&var_titles=<boo>"
OUT=$($CURL --silent $JSON_URL)
check_not_from "$OUT" fgrep -q "<boo>"
check_from "$OUT" fgrep -q "&lt;boo&gt;"

start_test Statistics console is available.
CONSOLE_URL=$PRIMARY_SERVER/pagespeed_console
CONSOLE_HTML=$OUTDIR/console.html
$WGET_DUMP $CONSOLE_URL > $CONSOLE_HTML
check grep -q "console" $CONSOLE_HTML
