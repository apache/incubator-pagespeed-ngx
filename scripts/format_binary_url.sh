#!/bin/bash

set -e
set -u

if [ $# -ne 1 ]; then
  echo "Usage: $(basename $0) <url_file>" >&2
  exit 1
fi

url_file=$1

if [ ! -e "$url_file" ]; then
  echo "Url file '$url_file' missing!" >&2
fi

# The size names must match install/build_psol.sh in mod_pagespeed
if [ "$(uname -m)" = x86_64 ]; then
  bit_size_name=x64
else
  bit_size_name=ia32
fi

sed -e 's/$BIT_SIZE_NAME\b/'$bit_size_name'/g' $url_file
