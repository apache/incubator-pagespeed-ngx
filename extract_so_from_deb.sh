#!/bin/sh
#
# Given a deb, extract mod_pagespeed.so and mod_pagespeed_ap24.so.  This is
# useful for running load tests on prior releases.  The files are left in a temp
# directory, and the path to them is printed to stdout.

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

if [ ! $# -eq 1 ]; then
  echo "Usage: ./extract_so_from_deb.sh mod-pagespeed-beta_current_amd64.deb"
  exit 1
fi

if [ ! -e $1 ]; then
  echo "File '$1' not found."
  exit 1
fi

input_deb=$(readlink -e $1)

TMP=$(mktemp -d)
cd "$TMP"
mkdir scratch
cd scratch

ar vx "$input_deb" > /dev/null
# all deb files have a data.tar.gz, which is now in the current directory.
tar -x --file=data.tar.gz \
    --wildcards ./usr/lib/apache2/modules/mod_pagespeed\*.so

mv usr/lib/apache2/modules/* ..
cd ..
rm -r scratch/

echo "The .so files are:"
for x in $PWD/*; do
  echo "  $x"
done | sort -r
