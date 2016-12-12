#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# Author: cheesy@google.com (Steve Hill)
#
# Locate and run the OS-specific version of a PageSpeed setup script.

# Note that these 'sets' are propagated to the caller in the case where
# this script is loaded via 'source'. This is by design.

set -e
set -u

my_dir="$(dirname "$BASH_SOURCE")"
target_basename="$(basename "$BASH_SOURCE")"

distro="$(lsb_release -is | tr A-Z a-z)"

if [ -z "$distro" ]; then
  echo "Could not determine distribution, is lsb_release installed?" >&2
  exit 1
fi

if [ ! -d "$my_dir/$distro" ]; then
  echo "$distro is not a supported build platform!" >&2
  exit 1
fi

target="$my_dir/$distro/$target_basename"

if [[ "$target" == *.sh ]]; then
  # All the "dependent" scripts are going to want these, so just load them here.
  source "$my_dir/shell_utils.sh" || exit 1
  source "$my_dir/$distro/build_env.sh" || exit 1
  source "$target" "$@"
else
  exec "$target" "$@"
fi
