#!/bin/bash
#
# Runs system tests for system/ and automatic/.
#
# See automatic/system_test_helpers.sh for usage.
#

# Run the automatic/ system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test.sh" || exit 1

# TODO(jefftk): move all tests from apache/system_test.sh to here except the
# ones that actually are Apache-specific.
