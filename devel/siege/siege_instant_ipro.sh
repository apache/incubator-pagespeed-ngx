#!/bin/sh

if ! hash siege 2>/dev/null; then
  echo "'siege' command is not found. Please install siege >=3.0.8."
  echo "Note that repository may contain older version of Siege."
  exit 1
fi

# Check that siege is at least 3.0.8.
siege_version=$(siege --version 2>&1 | head -n 1 | awk '{print $2}')
major=$(echo "$siege_version" | awk -F. '{print $1}')
minor=$(echo "$siege_version" | awk -F. '{print $2}')
point=$(echo "$siege_version" | awk -F. '{print $3}')

recent_siege=false
if [ "$major" -gt 3 ]; then
  recent_siege=true
elif [ "$major" -eq 3 ]; then
  if [ "$minor" -gt 0 ]; then
    recent_siege=true
  elif [ "$point" -ge 8 ]; then
    recent_siege=true
  fi
fi

if ! "$recent_siege"; then
  # Versions before 3.0.8 didn't include the port number in the host header.
  echo "$0: siege is version $siege_version but we need at least 3.0.8"
  exit 1
fi

this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/siege_helper.sh" || exit 1

# Make a list of urls large enough that we can run through them for at least
# 20s.
urls_file=$(mktemp /tmp/siege.urls.XXXXXX)
function remove_urls_file() {
  rm "$urls_file" 2> /dev/null
}
trap remove_urls_file EXIT
url_base="http://localhost:8080/mod_pagespeed_test/"
url_base+="ipro/instant/wait/purple.css?$RANDOM="
echo "Building url file..."
> "$urls_file"
N=100000
for i in $(seq 1 "$N"); do
  echo "$url_base$i" >> "$urls_file"
done

# The siege documentation suggests that --reps means how many times to run
# through the file of urls, but it's actually implemented as meaning the number
# of urls each of the concurrent processes should run through.  So if we have N
# urls to test and C processes, then each process should get N/C urls.
C=10
R="$(($N/$C))"
run_siege_with_options \
    --file="$urls_file" \
    --reps="$R" \
    --benchmark \
    --concurrent="$C"
