#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Build a mod_pagespeed release on a gcloud VM.
#
# You may want to set CLOUDSDK_COMPUTE_REGION and/or CLOUDSDK_CORE_PROJECT,
# or set your gcloud defaults befor running this:
# https://cloud.google.com/sdk/gcloud/reference/config/set

set -u

branch=master
delete_existing_machine=false
image_family=ubuntu-1204-lts
keep_machine=false
machine_name=
use_existing_machine=false

options="$(getopt --long build_branch:,centos,delete_existing_machine \
  --long image_family:,machine_name:,use_existing_machine \
  -o '' -- "$@")"
if [ $? -ne 0 ]; then
  echo "Usage: $(basename "$0") [options] -- [build_release.sh options]" >&2
  echo "  --build_branch=<branch>    mod_pagespeed branch to build" >&2
  echo "  --centos                   Shortcut for --image_family=centos-6" >&2
  echo "  --delete_existing_machine  If the VM already exists, delete it" >&2
  echo "  --image_family=<family>    Image family used to create VM" >&2
  echo "                             See: gcloud compute images list" >&2
  echo "  --machine_name             VM name to create" >&2
  echo "  --use_existing_machine     Re-run build on exiting VM" >&2
  exit 1
fi

set -e
eval set -- "$options"

while [ $# -gt 0 ]; do
  case "$1" in
    --build_branch) branch="$2"; shift 2 ;;
    --centos) image_family="centos-6"; shift 2 ;;
    --delete_existing_machine) delete_existing_machine=true; shift ;;
    --image_family) image_family="$2"; shift 2 ;;
    --keep_machine) keep_machine=true; shift ;;
    --machine_name) machine_name="$2"; shift 2 ;;
    --use_existing_machine) use_existing_machine=true; shift ;;
    --) shift; break ;;
    *) echo "getopt error" >&2; exit 1 ;;
  esac
done

if $use_existing_machine && $delete_existing_machine; then
  echo "Supply only one of --delete_existing_machine and" \
       "--use_existing_machine" >&2
  exit 1
fi

if ! type gcloud >/dev/null 2>&1; then
  echo "gcloud is not in your PATH. See: https://cloud.google.com/sdk/" >&2
  exit 1
fi

use_rpms=false

case "$image_family" in
  centos-*) image_project=centos-cloud ; use_rpms=true ;;
  ubuntu-*) image_project=ubuntu-os-cloud ;;
  *) echo "This script doesn't recognize image family '$image_family'" >&2;
     exit 1 ;;
esac

if [ -z "$machine_name" ]; then
  bit_suffix=
  for flag in "$@"; do
    if [ "$flag" = "--32bit" ]; then
      bit_suffix='-32'
      break
    fi
  done

  # gcloud doesn't allow dashes in machine names.
  sanitized_branch="$(tr _ - <<< "$branch")"

  machine_name="${USER}-${image_family}${bit_suffix}"
  machine_name+="-${sanitized_branch}-mps-build"
fi

instances=$(gcloud compute instances list -q "$machine_name")
if [ -n "$instances" ]; then
  if $delete_existing_machine; then
    gcloud -q compute instances delete "$machine_name"
    instances=
  elif ! $use_existing_machine; then
    echo "Instance '$machine_name' already exists." >&2
    exit 1
  fi
fi

if [ -z "$instances" ] || ! $use_existing_machine; then
  gcloud compute instances create "$machine_name" \
    --image-family="$image_family" --image-project="$image_project"
fi

mkdir -p ~/release

# Display an error including the machine name if we die in the script below.
trap '[ $? -ne 0 ] && echo -e "\nBuild failed on $machine_name"' EXIT

gcloud compute ssh "$machine_name" -- bash << EOF
  set -e
  set -x
  if $use_rpms; then
    sudo yum -y install git redhat-lsb
  else
    sudo apt-get -y install git
  fi
  git clone -b "$branch" https://github.com/pagespeed/mod_pagespeed.git
  cd mod_pagespeed
  install/build_release.sh $@
EOF

gcloud compute copy-files "${machine_name}:mod_pagespeed/release/*" ~/release/

if ! $keep_machine; then
  gcloud -q compute instances delete "$machine_name"
fi
