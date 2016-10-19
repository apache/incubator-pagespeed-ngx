start_test vhost inheritance works
echo $WGET_DUMP $SECONDARY_CONFIG_URL
SECONDARY_CONFIG=$($WGET_DUMP $SECONDARY_CONFIG_URL)
config_title="<title>PageSpeed Configuration</title>"
check_from "$SECONDARY_CONFIG" fgrep -q "$config_title"
# Sharding is applied in this host, thanks to global inherit flag.
check_from "$SECONDARY_CONFIG" egrep -q "http://nonspdy.example.com/"

# We should also inherit the blocking rewrite key.
check_from "$SECONDARY_CONFIG" egrep -q "\(blrw\)[[:space:]]+psatest"
