start_test If parsing
# $STATISTICS_URL ends in ?ModPagespeed=off, so we need & for now.
# If we remove the query from $STATISTICS_URL, s/&/?/.
readonly CONFIG_URL="$STATISTICS_URL&config"

echo $WGET_DUMP $CONFIG_URL
CONFIG=$($WGET_DUMP $CONFIG_URL)
config_title="<title>PageSpeed Configuration</title>"
check_from "$CONFIG" fgrep -q "$config_title"
# Regular config should have a shard line:
check_from "$CONFIG" egrep -q "http://nonspdy.example.com/ Auth Shards:{http:"
check_from "$CONFIG" egrep -q "//s1.example.com/, http://s2.example.com/}"
# And "combine CSS" on.
check_from "$CONFIG" egrep -q "Combine Css"
