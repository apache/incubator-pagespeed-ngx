#!/bin/bash
# Copyright 2016 Google Inc. All Rights Reserved.
# Author: cheesy@google.com (Steve Hill)
#
# Setup a 32-bit chroot for CentOS.

# This comes from build_env.sh.
if [ -z "${CHROOT_DIR:-}" ]; then
  echo "This must be run via os_redirector.sh!" >&2
  exit 1
fi

if [ -d "$CHROOT_DIR" ]; then
  "$install_dir/run_in_chroot.sh" /bin/true >/dev/null 2>&1
  if [ $? -eq 0 ]; then
    echo chroot already setup, nothing to do.
    exit 0
  else
    echo "$CHROOT_DIR exists but doesn't seem to be setup correctly." >&2
    echo "You're going to have to clean up manually." >&2
    exit 1
  fi
fi

if [ "$UID" != 0 ]; then
  echo "This script needs to run as root, re-running with sudo"
  exec sudo $0 "$@"
  exit 1  # NOTREACHED
fi

# TODO(cheesy): The release_rpm stuff below is not especially robust, but
# scraping the site is a pain. If this is a problem we can fix it later.

centos_version="$(lsb_release -rs)"
git_pkg=
if version_compare "$centos_version" -lt 6; then
  # CentOS 5
  release_rpm_url=http://mirror.centos.org/centos/5/os/i386/CentOS/centos-release-5-11.el5.centos.i386.rpm
elif version_compare "$centos_version" -lt 7; then
  # CentOS 6
  release_rpm_url=http://mirror.centos.org/centos/6/os/i386/Packages/centos-release-6-8.el6.centos.12.3.i686.rpm
  # TODO(cheesy): Once gclient is gone, we may be able to use the git rpm.
else
  # CentOS 7
  release_rpm_url=http://mirror.centos.org/altarch/7/os/i386/Packages/centos-release-7-2.1511.el7.centos.2.9.i686.rpm
  git_pkg=git
fi

release_rpm="$(basename "$release_rpm_url")"
wget -O "$release_rpm" "$release_rpm_url"

mkdir -p "$CHROOT_DIR/var/lib/rpm"

# Required to run the chroot in 32-bit mode.
yum -y install setarch

function cleanup_etc_rpm_platform() {
  if [ -s /etc/rpm/platform.real ]; then
    mv -f /etc/rpm/platform.real /etc/rpm/platform
  elif [ -f /etc/rpm/platform.real ]; then
    # Only do this if platform.real exists, otherwise we could delete a
    # perfectly valid file.
    rm -f /etc/rpm/platform /etc/rpm/platform.real
  fi
}

# To force install a different architecture, we must put a fake arch into
# /etc/rpm/platform. Older CentOSes will have the file, newer may not. Either
# way, it's important that we don't leave the fake one lying around.
trap 'cleanup_etc_rpm_platform'  EXIT
if [ -e /etc/rpm/platform ]; then
  mv /etc/rpm/platform /etc/rpm/platform.real
else
  touch /etc/rpm/platform.real
fi

echo i686-redhat-linux > /etc/rpm/platform
rpm --root="$CHROOT_DIR" --rebuilddb
rpm --root="$CHROOT_DIR" --nodeps -i "$release_rpm"

yum -y --installroot="$CHROOT_DIR" update
# redhat-lsb and sudo are required for run_in_chroot.sh
yum -y --installroot="$CHROOT_DIR" install yum sudo redhat-lsb

cleanup_etc_rpm_platform
trap - EXIT

for x in passwd shadow group gshadow hosts sudoers resolv.conf; do
  ln -f "/etc/$x" "$CHROOT_DIR/etc/$x"
done

cp -p /etc/yum.repos.d/* "$CHROOT_DIR/etc/yum.repos.d/"
# These don't work for 32-bit.
if version_compare "$centos_version" -ge 6; then
  rm -f "$CHROOT_DIR/etc/yum.repos.d"/CentOS-SCLo-scl[.-]*
fi

for dir in /proc /sys /dev /selinux /etc/selinux /home; do
  [ -d "$dir" ] || continue
  chroot_dir="${CHROOT_DIR}$dir"
  if [ ! -d "$chroot_dir" ]; then
    mkdir -p "$chroot_dir"
    chown --reference "$dir" "$chroot_dir"
    chmod --reference "$dir" "$chroot_dir"
  fi
  echo "$dir $chroot_dir none bind 0 0" >> /etc/fstab
done

echo "none $CHROOT_DIR/dev/shm tmpfs defaults 0 0" >> /etc/fstab

mount -a

# The previous yum install above probably did all the updates,
# but it doesn't hurt to ask.
install/run_in_chroot.sh yum -y update
install/run_in_chroot.sh yum -y install which redhat-lsb curl wget $git_pkg

if [ -z "$git_pkg" ]; then
  install/run_in_chroot.sh install/install_from_source.sh git
fi
