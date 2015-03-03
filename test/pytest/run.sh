#!/bin/bash

set -o nounset
set -o errexit


# Get the full path of this script's directory
MYDIR=$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )
WORKDIR="$MYDIR/../../.."

# defaults assume a root directory containing:
# $WORKDIR/ngx_pagespeed
# $WORKDIR/mod_pagespeed
# $WORKDIR/testing (nginx build directory, has sbin/nginx)
# If that's not the way things are on your system, you need
# to pass in MPS_DIR and/or NGINX_BINARY

MPS_DIR=${MPS_DIR:-"$WORKDIR/mod_pagespeed"}
NGINX_BINARY=${NGINX_BINARY:-"$WORKDIR/testing/sbin/nginx"}
PRIMARY_PORT=8050
SECONDARY_PORT=8051

# Expand to full paths for easier reading upon failure
MPS_DIR=$(readlink -f $MPS_DIR)
NGINX_BINARY=$(readlink -f $NGINX_BINARY)

if [ ! -d "$MPS_DIR" ]; then
    echo "mod_pagespeed directory not found: $MPS_DIR"
    exit -1
fi

if [ ! -f "$NGINX_BINARY" ]; then
    echo "nginx binary not found: $NGINX_BINARY"
    exit -1
fi

# Configure and fire up the nginx processes we'll test against
cd "$MYDIR/.."
PAGESPEED_TEST_HOST=ngxpagespeed.com RUN_TESTS=false \
  ./run_tests.sh $PRIMARY_PORT $SECONDARY_PORT \
  "$MPS_DIR" \
  "$NGINX_BINARY"

TEST_TMP_DIR="MYDIR/tmp" \
PRIMARY_SERVER="http://localhost:$PRIMARY_PORT" \
SECONDARY_SERVER="http://localhost:$SECONDARY_PORT" \
py.test "$MYDIR" "$@"

killall nginx
