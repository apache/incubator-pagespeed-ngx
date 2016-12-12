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
# Setup a 32-bit chroot for Ubuntu.

install_dir="$(dirname "${BASH_SOURCE[0]}")/.."

chroot_dir="/var/chroot/$CHROOT_NAME"

if [ -d "$chroot_dir" ]; then
  "$install_dir/run_in_chroot.sh" /bin/true >/dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo chroot already setup, nothing to do.
    exit 0
  else
    echo "$chroot_dir exists but doesn't seem to be setup correctly." >&2
    echo "You're going to have to clean up manually." >&2
    exit 1
  fi
fi

if [ "$UID" -ne 0 ]; then
  echo This script needs to run as root. Re-execing via sudo.
  exec sudo $0 "$@"
  exit 1  # NOTREACHED
fi

apt-get -y update
# We have to install ssl-cert in the host because /etc/group is copied
# into the chroot.
apt-get -y install debootstrap dchroot ssl-cert

distro_name="$(lsb_release -cs)"

# Create the initial chroot.
debootstrap --variant=buildd --arch i386 \
  "$distro_name" "$chroot_dir" http://archive.ubuntu.com/ubuntu/

# Stop daemons from starting in the chroot. Do this before updating any pkgs!
# https://major.io/2016/05/05/preventing-ubuntu-16-04-starting-daemons-package-installed/
cat > "$chroot_dir/usr/sbin/policy-rc.d" << EOF
#!/bin/sh
# Prevent all daemons from starting.
# Created by $(basename $0) for mod_pagespeed.
exit 101
EOF
chmod +x "$chroot_dir/usr/sbin/policy-rc.d"

# Configure schroot

cat >> /etc/schroot/schroot.conf << EOF
[$CHROOT_NAME]
description=Ubuntu $distro_name for i386
directory=$chroot_dir
type=directory
personality=linux32
preserve-environment=true
root-groups=sudo
groups=sudo
# This may expand to empty, which is OK.
root-users=${SUDO_USER:-}
users=${SUDO_USER:-}
EOF

cat >> /etc/schroot/default/fstab << EOF
# Don't add /dev/shm, weird things will happen. See:
# https://bugs.launchpad.net/ubuntu/+source/schroot/+bug/1438942/comments/5
none   /run/shm   tmpfs   rw,nosuid,nodev,noexec 0 0
EOF

cat >> /etc/schroot/default/copyfiles << EOF
/etc/apt/sources.list
EOF

# schroot is now functional, so we can use run_in_chroot.sh to complete setup
# of the chroot.

"$install_dir/run_in_chroot.sh" apt-get -y update
"$install_dir/run_in_chroot.sh" apt-get -y upgrade
"$install_dir/run_in_chroot.sh" apt-get -y install locales sudo lsb-release
"$install_dir/run_in_chroot.sh" locale-gen en_US.UTF-8

# This must be done after we install sudo or dpkg gets cranky.
echo /etc/sudoers >> /etc/schroot/default/copyfiles
