#!/bin/bash
#
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
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

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test "Starting up pathological server on $RCPORT"
  "$this_dir/pathological_server.py" "$RCPORT" &
  server_pid=$!
  trap "kill $server_pid" EXIT

  echo "Server running with pid $server_pid"
  echo "Verifying that the server came up"
  sleep 1  # Give the server time to come up.
  check "$CURL" -sS -D- "localhost:$RCPORT/standard"

  if ! "${SKIP_FILESYSTEM_WRITE_ACCESS_TESTS:-false}" && \
     [ "$SERVER_NAME" = "apache" ] ; then
    start_test remote config will not apply server scoped options.
    # Check that server scoped options are ignored, but directory scoped options
    # are applied.
    URL="$(generate_url remote-config-out-of-scope.example.com \
           /mod_pagespeed_test/forbidden.html)"
    echo wget $URL $SECONDARY_HOSTNAME $WGET_DUMP
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
    if [ -s $APACHE_LOG ]; then
      check_from "$(cat $APACHE_LOG)" \
        grep "Setting option UrlSigningKey with value secretkey failed"
      check_not_from "$(cat $APACHE_LOG)" \
        grep "Setting option RequestOptionOverride with value secretkey failed"
    fi

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
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

    start_test htaccess is overridded by remote configuration file.
    RCPATH="/mod_pagespeed_test/remote_config/remote.cfg.enable_comments"
    PORT=$(echo $SECONDARY_HOSTNAME | cut -d \: -f 2)
    echo "ModPagespeedEnableFilters remove_comments,collapse_whitespace" > \
      $APACHE_DOC_ROOT/mod_pagespeed_test/remote_config/withhtaccess/.htaccess
    URL="$(generate_url remote-config-with-htaccess.example.com \
           /mod_pagespeed_test/remote_config/withhtaccess/remotecfgtest.html)"
    echo wget $URL
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0
    echo "ModPagespeedEnableFilters remove_comments,collapse_whitespace"  > $HTA
    echo "ModPagespeedRemoteConfigurationUrl \"$HOST:$PORT$RCPATH\"" >> $HTA
    URL="$(generate_url remote-config-with-htaccess.example.com \
           /mod_pagespeed_test/remote_config/withhtaccess/remotecfgtest.html)"
    echo wget $URL
    http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 2
    rm -f $HTA
  fi

  # Tests for remote configuration files.
  # The remote configuration location is stored in the debug.conf.template.
  start_test Remote Configuration On: by default comments and whitespace removed
  URL="$(generate_url remote-config.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

  start_test Remote Configuration On: File missing end token.
  URL="$(generate_url remote-config-invalid.example.com \
                      /mod_pagespeed_test/forbidden.html)"

  echo wget $URL
  # Fetch a few times to be satisfied that the configuration should be fetched.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"

  start_test Remote Configuration On: Some invalid options.
  URL="$(generate_url remote-config-partially-invalid.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  # Some options are invalid, check that they are skipped and the rest of the
  # options get applied.
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

  start_test Remote Configuration On: overridden by query parameter.
  URL="$(generate_url remote-config.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  # First check to see that the remote config is applied.
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0
  # And now check to see that the query parameters override the remote config.
  URL="$(generate_url remote-config.example.com \
      /mod_pagespeed_test/forbidden.html?PageSpeedFilters=-remove_comments)"
  echo wget $URL
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 2

  start_test second remote config fetch fails, cached value still applies.
  URL="$(generate_url remote-config-failed-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0
  echo "Sleeping so that cache will expire"
  sleep 2
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

  start_test config takes too long to fetch, is not applied.
  URL="$(generate_url remote-config-slow-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  # Fetch a few times to be satisfied that the configuration should have been
  # fetched.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"

  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" grep "<!--"

  start_test Remote Configuration specify an experiment.
  # Remote config url is configured to sleep an impossibly long time.
  URL="$(generate_url remote-config-experiment.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  # Some options are invalid, check that they are skipped and the rest of the
  # options get applied.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c MyExperimentID' 1

  start_test config takes a long time to fetch but is still applied.
  # Remote config url is configured to sleep for 2s.  This depends on the "sleep
  # 2" above.  If it were removed and everything else ran instantly then we
  # might not have a cached copy of the remote config file here.
  URL="$(generate_url remote-config-slightly-slow-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

  start_test config is expired, but is still applied.
  # Remote config url is configured to sleep for 2s, and returns expired
  # content.  This also depends on the "sleep 2" above.
  URL="$(generate_url remote-config-slightly-slow-expired-fetch.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  # TODO(jefftk): we doc that:
  #   The remote configuration file will be fetched on the server's startup, and
  #   cached for the extent determined by the remote server's Cache-Control and
  #   Expires headers. For example, if the remote configuration hosting server
  #   provides the header Cache-Control: max-age=3600, the next fetch of the
  #   remote configuration will happen at the first request after 3600
  #   seconds. Failed fetches after successful fetches will continue to serve
  #   the stale config.
  # but it doesn't look like we use the stale config here.
  # http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" 'fgrep -c <!--' 0

  start_test config is forbidden, and so never applied
  # Remote config url 403s, but returns valid remote config.  Still not applied.
  URL="$(generate_url remote-config-forbidden.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_not_from "$OUT" grep "<!--"

  start_test config is forbidden only on first request
  # Remote config url 403s the first request and 200s after that, so it should
  # be applied.
  URL="$(generate_url remote-config-forbidden.example.com \
                      /mod_pagespeed_test/forbidden.html)"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_not_from "$OUT" grep "<!--"

fi

check_failures_and_exit
