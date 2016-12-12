#!/bin/bash
#
# Copyright 2010 Google Inc.
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
# Processes the open-source header files using Doxygen.  Each header
# must be preprocessed using doxify.sh to convert normal C++ comments
# into Doxygen Usage.
#
# comments:  devel/doxify_tree.sh output_tarball

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

this_dir=$(dirname "${BASH_SOURCE[0]}")
cd "$this_dir/.."
src="$PWD"
cfg="$src/devel/doxygen.cfg"

if [ $# != 1 ]; then
  echo Usage: $0 output_tarball
  exit 1
fi

if ! which doxygen > /dev/null; then
  echo "doxygen is not installed; run"
  echo "  sudo apt-get install doxygen"
  exit 1
fi

# This generates a documentation tarball, suitable for copying to
# modpagespeed.com.  This should be in $1
tarball="$(readlink -f $1)"

source "$src/net/instaweb/public/VERSION"
PSOL_VERSION="$MAJOR.$MINOR.$BUILD.$PATCH"

WORKDIR=$(mktemp -d)
trap "rm -r $WORKDIR" EXIT

OUTPUT_DIRECTORY="$WORKDIR/doxygen_out"
mkdir "$OUTPUT_DIRECTORY"

hacked_copies="$WORKDIR/hacked_copies"
mkdir "$hacked_copies"

echo Preprocessing header files to turn normal C++ comments into Doxygen-style
echo comments....
find net/ pagespeed/ -name "*.h" -exec "$src/devel/doxify.sh" {} \
     "$hacked_copies" \;

# These variables are referenced in doxygen.cfg, so export them before running
# doxygen.
export PSOL_VERSION
export OUTPUT_DIRECTORY

log_file=$OUTPUT_DIRECTORY/doxygen.log
cd $hacked_copies
doxygen $cfg 2> $log_file

# Doxygen produces a large number of warnings about undocumented classes.  At
# some point we should fix all these but this is going to take a while as there
# are 12431 as of 2016-11-18.
#
# These will reference files that we have hacked in this script, and using Emacs
# to navigate to these errors will get you to files you should never edit.
# Strip off the prefix so we'll print files with their source of truth.
grep hacked_copies $log_file | sed -e s@$hacked_copies/@@g

# TODO(jmarantz): walk through files in $OUTPUT_DIRECTORY/html and see whether
# there are changes to the corresponding files in the documentation.
cd $OUTPUT_DIRECTORY
tar czf $tarball .
ls -l $tarball
