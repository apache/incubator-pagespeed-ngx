#!/bin/sh
# Runs 'siege' on a single ipro-optimized image.
#
# Usage:
#   devel/siege/siege_ipro_image.sh

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

echo "Waiting for the image to be IPRO-optimized..."
URL="http://localhost:8080/mod_pagespeed_example/images/Puzzle.jpg"

while true; do
  content_length=$(curl -sS -D- -o /dev/null "$URL" | \
     grep '^Content-Length: ' | \
     grep -o '[0-9]*')
  if [ "$content_length" -lt 100000 ]; then
    # the image is fully ipro optimized
    break
  fi
  sleep .1
  echo -n .
done

run_siege "$URL"
