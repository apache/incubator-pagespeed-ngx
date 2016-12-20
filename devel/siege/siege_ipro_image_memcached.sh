#!/bin/sh
# Runs 'siege' on a single ipro-optimized image with memcached.
#
# Usage:
#   devel/siege/siege_ipro_image_memcached.sh

this_dir=$(readlink -e "$(dirname "${BASH_SOURCE[0]}")")
root_dir=$(readlink -e "$this_dir/../..")
install_dir="$root_dir/install"

set -e
"$install_dir/run_program_with_memcached.sh" "$this_dir/siege_ipro_image.sh"
