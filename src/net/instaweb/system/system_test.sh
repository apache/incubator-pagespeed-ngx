#!/bin/bash
#
# Runs system tests for system/ and automatic/.
#
# See automatic/system_test_helpers.sh for usage.
#

# Run the automatic/ system tests.
this_dir=$(dirname $0)
source "$this_dir/../automatic/system_test.sh" || exit 1

# TODO(jefftk): move all tests from apache/system_test.sh to here except the
# ones that actually are Apache-specific.
