#!/bin/bash
#
# Run the remote config tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")

# This script is invoked in two ways:
#   1. run from apache_debug_remote_config_test.
#   2. sourced from nginx_system_test.sh, after sourcing system/system_test.sh.
# In the latter case, system_test_helpers.sh is already loaded, and it doesn't
# work to load it twice.
if [ "${HELPERS_LOADED:-0}" != 1 ]; then
  source "$this_dir/../automatic/system_test_helpers.sh" || exit 1
fi

start_test remote config will not apply server scoped options.
if ! [ -z ${RCPORT4+x} ] && [ $RCPORT4 -eq "9994" ] && \
    [ $SERVER_NAME = "apache"] ; then
  # These tests can only be done if the port is known ahead of time.
  # Check that server scoped options are ignored, but directory scoped options
  # are applied.
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    URL="$(generate_url remote-config-out-of-scope.example.com \
           /mod_pagespeed_test/forbidden.html)"
    while true; do
      echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nUrlSigningKey secretkey\nRequestOptionOverride secretkey\nEndRemoteConfig\n" | nc -l -p $RCPORT4 -q 1
    done&
    LOOPPID=$!
    echo wget $URL $SECONDARY_HOSTNAME $WGET_DUMP
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
    if [ -s $APACHE_LOG ]; then
      check_from "$(cat $APACHE_LOG)" grep "Setting option UrlSigningKey with value secretkey failed"
      check_not_from "$(cat $APACHE_LOG)" grep "Setting option RequestOptionOverride with value secretkey failed"
    fi
    kill $LOOPPID
    kill_port $RCPORT4

    HTA=$APACHE_DOC_ROOT/mod_pagespeed_test/remote_config/withhtaccess/.htaccess
    start_test htaccess references a remote configuration file.
    # First, check that the remote configuration is applied via .htaccess files.
    # PORT isn't known until the test is run, so we must extract it from the
    # $SECONDARY_HOSTNAME variable.
    PORT=$(echo $SECONDARY_HOSTNAME | cut -d \: -f 2)
    HOST="http://localhost"
    RCPATH="/mod_pagespeed_test/remote_config/remote.cfg"
    echo "ModPagespeedRemoteConfigurationUrl \"$HOST:$PORT$RCPATH\"" > $HTA
    URL="$(generate_url remote-config-with-htaccess.example.com \
           /mod_pagespeed_test/remote_config/withhtaccess/remotecfgtest.html)"
    echo wget $URL
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
        'fgrep -c <!--' 0

    start_test htaccess is overridded by remote configuration file.
    RCPATH="/mod_pagespeed_test/remote_config/remote.cfg.enable_comments"
    PORT=$(echo $SECONDARY_HOSTNAME | cut -d \: -f 2)
    echo "ModPagespeedEnableFilters remove_comments,collapse_whitespace" > \
      $APACHE_DOC_ROOT/mod_pagespeed_test/remote_config/withhtaccess/.htaccess
    URL="$(generate_url remote-config-with-htaccess.example.com \
           /mod_pagespeed_test/remote_config/withhtaccess/remotecfgtest.html)"
    echo wget $URL
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
        'fgrep -c <!--' 0
    echo "ModPagespeedEnableFilters remove_comments,collapse_whitespace"  > $HTA
    echo "ModPagespeedRemoteConfigurationUrl \"$HOST:$PORT$RCPATH\"" >> $HTA
    URL="$(generate_url remote-config-with-htaccess.example.com \
           /mod_pagespeed_test/remote_config/withhtaccess/remotecfgtest.html)"
    echo wget $URL
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
        'fgrep -c <!--' 2
    rm -f $HTA
  fi
fi

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  # Tests for remote configuration files.
  # The remote configuration location is stored in the debug.conf.template.
  start_test Remote Configuration On: by default comments and whitespace removed
  URL="$(generate_url remote-config.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT1
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nEnableFilters remove_comments,collapse_whitespace\nEndRemoteConfig\n" | nc -l -p $RCPORT1 -q 1
  done &
  LOOPPID=$!
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  kill $LOOPPID
  kill_listener_port nc $RCPORT1

  start_test Remote Configuration On: File missing end token.
  URL="$(generate_url remote-config-invalid.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT3
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nEnableFilters remove_comments,collapse_whitespace\n" | nc -l -p $RCPORT3 -q 1
  done &
  LOOPPID=$!
  echo wget $URL
  # Fetch a few times to be satisfied that the configuration should be fetched.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  kill $LOOPPID
  kill_listener_port nc $RCPORT3

  start_test Remote Configuration On: Some invalid options.
  URL="$(generate_url remote-config-partially-invalid.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT2
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nEnableFilters remove_comments,collapse_whitespace\nEndRemoteConfig\n" | nc  -l -p $RCPORT2 -q 1
  done&
  LOOPPID=$!
  # Some options are invalid, check that they are skipped and the rest of the
  # options get applied.
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  kill $LOOPPID
  kill_listener_port nc $RCPORT2

  start_test Remote Configuration On: overridden by query parameter.
  URL="$(generate_url remote-config.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT1
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nEnableFilters remove_comments,collapse_whitespace\nEndRemoteConfig\n" | nc -l -p $RCPORT1 -q 1
  done &
  LOOPPID=$!
  # First check to see that the remote config is applied.
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  # And now check to see that the query parameters override the remote config.
  URL="$(generate_url remote-config.example.com \
      /mod_pagespeed_test/forbidden.html?PageSpeedFilters=-remove_comments)"
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 2
  kill $LOOPPID
  NCPID="$(lsof -i:$RCPORT1 -t)" || true
  kill_listener_port nc $RCPORT1

  start_test second remote config fetch fails, cached value still applies.
  kill_listener_port nc $RCPORT5
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=1\n\nEnableFilters remove_comments,collapse_whitespace\nEndRemoteConfig\n" | nc -l -p $RCPORT5 -q 1
  done &
  LOOPPID=$!
  URL="$(generate_url remote-config-failed-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  echo "Sleeping so that cache will expire"
  sleep 2
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c <!--' 0
  kill $LOOPPID
  kill_listener_port nc $RCPORT5

  start_test config takes too long to fetch, is not applied.
  URL="$(generate_url remote-config-slow-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT6
  while true; do
    sleep 4; echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=2\n\nEnableFilters remove_comments,collapse_whitespace\nEndRemoteConfig\n" | nc -l -p $RCPORT6 -q 4
  done &
  LOOPPID=$!
  # Fetch a few times to be satisfied that the configuration should have been
  # fetched.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  kill $LOOPPID
  kill_listener_port nc $RCPORT6

  start_test Remote Configuration specify an experiment.
  URL="$(generate_url remote-config-experiment.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  kill_listener_port nc $RCPORT7
  while true; do
    echo -e "HTTP/1.1 200 OK\nCache-Control: max-age=5\n\nRunExperiment on\nAnalyticsID UA-MyExperimentID-1\nUseAnalyticsJs false\nEndRemoteConfig\n" | nc -l -p $RCPORT7 -q 1
  done &
  LOOPPID=$!
  # Some options are invalid, check that they are skipped and the rest of the
  # options get applied.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c MyExperimentID' 1
  kill $LOOPPID
  kill_listener_port nc $RCPORT7
fi

check_failures_and_exit
