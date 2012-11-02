#!/bin/bash
# Author: jefftk@google.com (Jeff Kaufman)
#
# Runs pagespeed's generic system test.
#
# Exits with status 0 if all tests pass.
# Exits with status 1 immediately if any test fails.
# Exits with status 2 if command line args are wrong.
#
# Usage:
#   Set up nginx to serve mod_pagespeed/src/install/ statically at the server
#   root, then run:
#     ./ngx_system_test.sh HOST:PORT
#   for example:
#     ./ngx_system_test.sh localhost:8050
#


# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir="$( dirname "${BASH_SOURCE[0]}" )"


SYSTEM_TEST_FILE="$this_dir/../../mod_pagespeed/src/install/system_test.sh"

if [ ! -e "$SYSTEM_TEST_FILE" ] ; then
  echo "Not finding $SYSTEM_TEST_FILE -- is mod_pagespeed not in a parallel"
  echo "directory to ngx_pagespeed?"
  exit 2
fi

source $SYSTEM_TEST_FILE
