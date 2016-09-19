#!/bin/bash

function usage() {
  echo "
Usage: build_ngx_pagespeed.sh [options]

  Installs ngx_pagespeed and its dependencies.  Can optionally build and install
  nginx as well.

Options:
  -v, --ngx-pagespeed-version <ngx_pagespeed version>
      What version of ngx_pagespeed to build.  Required.

  -n, --nginx-version <nginx version>
      What version of nginx to build.  If not set, this script only prepares the
      ngx_pagespeed module, and expects you to handle including it when you
      build nginx.

  -b, --builddir <directory>
      Where to build.  Defaults to \$HOME.

  -p, --no-deps-check
      By default, this script checks for the packages it depends on and tries to
      install them.  If you have installed dependencies from source or are on a
      non-deb non-rpm system, this won't work.  In that case, install the
      dependencies yourself and pass --no-deps-check.

  -d, --dryrun
      Don't make any changes to the system, just print what changes you
      would have made.

  -h, --help
      Print this message and exit."
}

function pass_h_for_usage() {
  # Normally I'd use $0 here, but since most people will be running this via
  # curl, that wouldn't actually give something useful.
  echo
  echo "For usage information, run this script with --help"
}

# Intended to be called as:
#   bash <(curl dl.google.com/.../build_ngx_pagespeed.sh) <args>

# If we set -e or -u then users of this script will see it silently exit on
# failure.  Instead we need to check the exit status of each command manually.
# The run function handles exit-status checking for system-changing commands.
# Additionally, this allows us to easily have a dryrun mode where we don't
# actually make any changes.
function run() {
  if "$DRYRUN"; then
    echo "would run $@"
  else
    if ! "$@"; then
      echo "Failure running $@, exiting."
      exit 1
    fi
  fi
}

function redhat_is_installed() {
  local package_name="$1"
  rpm -qa $package_name | grep -q .
}

function debian_is_installed() {
  local package_name="$1"
  dpkg -l $package_name | grep ^ii | grep -q .
}

# Usage:
#  install_dependencies install_pkg_cmd is_pkg_installed_cmd dep1 dep2 ...
#
# install_pkg_cmd is a command to install a dependency
# is_pkg_installed_cmd is a command that returns true if the dependency is
#   already installed
# each dependency is a package name
function install_dependencies() {
  local install_pkg_cmd="$1"
  local is_pkg_installed_cmd="$2"
  shift 2

  local missing_dependencies=""

  for package_name in "$@"; do
    if ! $is_pkg_installed_cmd $package_name; then
      missing_dependencies+="$package_name "
    fi
  done
  if [ -n "$missing_dependencies" ]; then
    echo "Detected that we're missing the following depencencies:"
    echo "  $missing_dependencies"
    echo "Installing them:"
    run sudo $install_pkg_cmd $missing_dependencies
  fi
}

function gcc_too_old() {
  # We need gcc >= 4.8
  local gcc_major_version=$(gcc -dumpversion | awk -F. '{print $1}')
  if [ "$gcc_major_version" -lt 4 ]; then
    return 0  # too old
  elif [ "$gcc_major_version" -gt 4 ]; then
    return 1  # plenty new
  fi
  # It's gcc 4.x, check if x >= 8:
  local gcc_minor_version=$(gcc -dumpversion | awk -F. '{print $2}')
  test "$gcc_minor_version" -lt 8
}

function continue_or_exit() {
  local prompt="$1"
  read -p "$prompt [Y/n] " yn
  if [[ "$yn" == N* || "$yn" == n* ]]; then
    echo "Cancelled."
    exit 0
  fi
}

function build_ngx_pagespeed() {
  getopt --test
  if [ "$?" != 4 ]; then
    echo "Your version of getopt is too old.  Exiting with no changes made."
    # Even Centos 5 and Ubuntu 10 LTS have new-style getopt, so I don't expect
    # this to be hit in practice on systems that are actually able to run
    # PageSpeed.
    exit 1
  fi

  opts=$(getopt -o v:n:b:pdh \
    --longoptions ngx-pagespeed-version:,nginx-version:,buildir:,no-deps-check \
    --longoptions dryrun,help \
    -n "$(basename "$0")" -- "$@")
  if [ $? != 0 ]; then
    usage
    exit 1
  fi
  eval set -- "$opts"

  NPS_VERSION=""
  NGINX_VERSION=""
  BUILDDIR="$HOME"
  DO_DEPS_CHECK=true
  DRYRUN=false
  while true; do
    case "$1" in
      -v | --ngx-pagespeed-version) shift
        NPS_VERSION="$1"
        shift
        ;;
      -n | --nginx-version) shift
        NGINX_VERSION="$1"
        shift
        ;;
      -b | --builddir) shift
        BUILDDIR="$1"
        shift
        ;;
      -p | --no-deps-check) shift
        DO_DEPS_CHECK=false
        ;;
      -d | --dryrun) shift
        DRYRUN="true"
        ;;
      -h | --help) shift
        usage
        exit 0
        ;;
      --) shift
        break
        ;;
      *)
        echo "Invalid argument: $1"
        usage
        exit 1
        ;;
    esac
  done

  if [ -z "$NPS_VERSION" ]; then
    echo "Please pass --ngx-pagespeed-version <version>"
    pass_h_for_usage
    exit 1
  fi

  if [ ! -d "$BUILDDIR" ]; then
    echo "Told to build in $BUILDDIR, but that directory doesn't exist."
    pass_h_for_usage
    exit 1
  fi

  # In our instructions we give a demo with 0.0.1 as an "obviously invalid"
  # nginx version number.  If someone copies and pastes the command as is, we
  # should give a friendly error message.
  if [ "$NGINX_VERSION" = "0.0.1" ]; then
    echo "You passed 0.0.1 for the version of nginx, but 0.0.1 is just a"
    echo "placeholder.  Check http://nginx.org/en/download.html for the"
    echo "latest version of nginx, and replace '0.0.1' with that."
    pass_h_for_usage
    exit 1
  fi

  if "$DRYRUN"; then
    TEMPDIR="/tmp/output-of-mktemp"
  else
    TEMPDIR=$(mktemp -d)
    function cleanup_tempdir {
      rm -rf "$TEMPDIR"
    }
    trap cleanup_tempdir EXIT
  fi

  PS_NGX_EXTRA_FLAGS=""

  # Now make sure our dependencies are installed.
  if "$DO_DEPS_CHECK"; then
    if [ -f /etc/debian_version ]; then
      echo "Detected debian-based distro."

      install_dependencies "apt-get install" debian_is_installed \
        "build-essential zlib1g-dev libpcre3 libpcre3-dev unzip"

      if gcc_too_old; then
        if [ ! -e /usr/lib/gcc-mozilla/bin/gcc ]; then
          echo "Detected that gcc is older than 4.8.  Installing gcc-mozilla"
          echo "which installs gcc-4.8 into /usr/lib/gcc-mozilla/ and doesn't"
          echo "affect your global gcc installation."
          run sudo apt-get install gcc-mozilla
        fi

        PS_NGX_EXTRA_FLAGS="--with-cc=/usr/lib/gcc-mozilla/bin/gcc"
        PS_NGX_EXTRA_FLAGS+=" --with-ld-opt=-static-libstdc++"
      fi

    elif [ -f /etc/redhat-release ]; then
      echo "Detected redhat-based distro."

      install_dependencies "yum install" redhat_is_installed \
        "gcc-c++ pcre-devel zlib-devel make unzip wget"
      if gcc_too_old; then
        if [ ! -e /opt/rh/devtoolset-2/root/usr/bin/gcc ]; then
          redhat_major_version=$(
            cat /etc/redhat-release | grep -o -E '[0-9]+' | head -n 1)
          if [ "$redhat_major_version" == 5 ]; then
            slc_version=5
          elif [ "$redhat_major_version" == 6 ]; then
            slc_version=6
          else
            echo "Unexpected major version $redhat_major_version in"
            echo "/etc/redhat-release: $(cat /etc/redhat-release)"
            echo "Expected 5 or 6."
            exit 1
          fi

          echo "Detected that gcc is older than 4.8.  Scientific Linux provides"
          echo "a gcc package that installs gcc-4.8 into /opt/ and doesn't"
          echo "affect your global gcc installation."
          slc_key="https://linux.web.cern.ch/linux/scientific6/docs/repository/"
          slc_key+="cern/slc6X/i386/RPM-GPG-KEY-cern"
          slc_key_out="$TEMPDIR/RPM-GPG-KEY-cern"
          run sudo wget "$slc_key" -O "$slc_key_out"
          run sudo rpm --import "$slc_key_out"

          repo_fname="/etc/yum.repos.d/slc${slc_version}-devtoolset.repo"
          if [ -e "$repo_fname" ]; then
            echo "Expected $repo_fname not to exist; aborting."
            exit 1
          fi

          repo_url="https://linux.web.cern.ch/linux/scientific${slc_version}/"
          repo_url+="/docs/repository/cern/devtoolset/"
          repo_url+="slc${slc_version}-devtoolset.repo"
          run sudo wget -O "$repo_fname" "$repo_url"
          run sudo yum install devtoolset-2-gcc-c++ devtoolset-2-binutils
        fi
        PS_NGX_EXTRA_FLAGS="--with-cc=/opt/rh/devtoolset-2/root/usr/bin/gcc"
      fi
    else
      echo "This doesn't appear to be a deb-based distro or an rpm-based one."
      echo "Not going to be able to install dependencies.  Please install"
      echo "dependencies manually and rerun with --depsinstalled."
      exit 1
    fi
    echo "Dependencies are all set."
  else
    echo "Not checking whether dependencies are installed."
  fi

  function delete_if_already_exists() {
    local directory="$1"
    if [ -d "$directory" ]; then
      if [ ${#directory} -lt 8 ]; then
        echo "Not deleting $directory; name is suspiciously short.  Something"
        echo "is wrong."
        exit 1
      fi

      continue_or_exit "OK to delete $directory?"
      rm -rf "$directory"
    fi
  }

  nps_baseurl="https://github.com/pagespeed/ngx_pagespeed/archive"
  nps_fname="release-${NPS_VERSION}-beta"
  nps_downloaded="$TEMPDIR/$nps_fname.zip"
  run wget "$nps_baseurl/$nps_fname.zip" -O "$nps_downloaded"
  nps_module_dir="$BUILDDIR/ngx_pagespeed-$nps_fname"
  delete_if_already_exists "$nps_module_dir"
  echo "Extracting ngx_pagespeed..."
  run unzip -q "$nps_downloaded" -d "$BUILDDIR"
  run cd "$nps_module_dir"
  run wget "https://dl.google.com/dl/page-speed/psol/${NPS_VERSION}.tar.gz"
  echo "Extracting PSOL..."
  run tar -xzf ${NPS_VERSION}.tar.gz  # extracts to psol/

  configure_args="--add-module=$nps_module_dir $PS_NGX_EXTRA_FLAGS"
  if [ -z "$NGINX_VERSION" ]; then
    # They didn't specify an nginx version, so we're just preparing the
    # module for them to install.
    echo "ngx_pagespeed is ready to be installed."
    echo "When running ./configure pass in:"
    echo "  $configure_args"
  else
    # Download and build nginx.
    nginx_leaf="nginx-${NGINX_VERSION}.tar.gz"
    nginx_fname="$TEMPDIR/$nginx_leaf"
    run wget "http://nginx.org/download/$nginx_leaf" -O "$nginx_fname"
    nginx_dir="$BUILDDIR/nginx-${NGINX_VERSION}/"
    delete_if_already_exists "$nginx_dir"
    echo "Extracting nginx..."
    run tar -xzf "$nginx_fname" --directory "$BUILDDIR"
    cd "$nginx_dir"

    echo "About to build nginx.  Do you have any additional ./configure"
    echo "arguments you would like to set?  For example, if you would like"
    echo "to build nginx with https support give --with-http_ssl_module"
    echo "If you don't have any, just press enter."
    read -p "> " additional_configure_args

    configure="./configure $configure_args $additional_configure_args"
    echo "About to configure nginx with:"
    echo "  $configure"
    continue_or_exit "Does this look right?"
    run $configure

    continue_or_exit "Build nginx?"
    run make

    continue_or_exit "Install nginx?"
    run sudo make install

    echo
    echo "Nginx installed with ngx_pagespeed support."
    echo
    echo "If this is a new installation you probably need an init script to"
    echo "manage starting and stopping the nginx service.  See:"
    echo "  https://www.nginx.com/resources/wiki/start/topics/examples/initscripts/"
    echo
    echo "You'll also need to configure ngx_pagespeed if you haven't yet:"
    echo "  https://developers.google.com/speed/pagespeed/module/configuration"
  fi
}

# Start running things from a call at the end so if this script is executed
# after a partial download it doesn't do anything.
build_ngx_pagespeed "$@"
