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

function usage() {
  echo "
Usage: build_ngx_pagespeed.sh [options]

  Installs ngx_pagespeed and its dependencies.  Can optionally build and install
  nginx as well.  Can be run either as:

     bash <(curl -f -L -sS https://ngxpagespeed.com/install) [options]

  Or:

     git clone git@github.com:pagespeed/ngx_pagespeed.git
     cd ngx_pagespeed/
     git checkout <branch>
     scripts/build_ngx_pagespeed.sh [options]

Options:
  -v, --ngx-pagespeed-version <ngx_pagespeed version>
      What version of ngx_pagespeed to build.  Valid options include:
      * latest-beta
      * latest-stable
      * a version number, such as 1.11.33.4

      If you don't specify a version, defaults to latest-stable unless --devel
      is specified, in which case it defaults to master.

      This option doesn't make sense if we're running within an existing
      ngx_pagespeed checkout.

  -n, --nginx-version <nginx version>
      What version of nginx to build.  If not set, this script only prepares the
      ngx_pagespeed module, and expects you to handle including it when you
      build nginx.

      If you pass in 'latest' then this script scrapes the nginx download page
      and attempts to determine the latest version automatically.

  -m, --dynamic-module
      Build ngx_pagespeed as a dynamic module.

  -b, --builddir <directory>
      Where to build.  Defaults to \$HOME.

  -p, --no-deps-check
      By default, this script checks for the packages it depends on and tries to
      install them.  If you have installed dependencies from source or are on a
      non-deb non-rpm system, this won't work.  In that case, install the
      dependencies yourself and pass --no-deps-check.

  -s, --psol-from-source
      Build PSOL from source instead of downloading a pre-built binary module.

  -l, --devel
      Sets up a development environment in ngx_pagespeed/nginx, building with
      testing-only dependencies.  Includes --psol-from-source, conflicts with
      --nginx-version.  Uses a 'git clone' checkout for ngx_pagespeed and nginx
      instead of downloading a tarball.

  -t, --build-type
      When building PSOL from source, what to tell it for BUILD_TYPE.  Defaults
      to 'Release' unless --devel is set in which case it defaults to 'Debug'.

  -y, --assume-yes
      Assume the answer to all prompts is 'yes, please continue'.  Intended for
      automated usage, such as buildbots.

  -a, --additional-nginx-configure-arguments
      When running ./configure for nginx, you may want to specify additional
      arguments, such as --with-http_ssl_module.  By default this script will
      pause and prompt you for them, but this option lets you pass them in.  For
      example, you might do:
        -a '--with-http_ssl_module --with-cc-opt=\"-I /usr/local/include\"'

  -d, --dryrun
      Don't make any changes to the system, just print what changes you
      would have made.

  -h, --help
      Print this message and exit."
}

RED=31
GREEN=32
YELLOW=33
function begin_color() {
  color="$1"
  echo -e -n "\e[${color}m"
}
function end_color() {
  echo -e -n "\e[0m"
}

function echo_color() {
  color="$1"
  shift
  begin_color "$color"
  echo "$@"
  end_color
}

function error() {
  local error_message="$@"
  echo_color "$RED" -n "Error: " >&2
  echo "$@" >&2
}

# Prints an error message and exits with an error code.
function fail() {
  error "$@"

  # Normally I'd use $0 in "usage" here, but since most people will be running
  # this via curl, that wouldn't actually give something useful.
  echo >&2
  echo "For usage information, run this script with --help" >&2
  exit 1
}


function status() {
  echo_color "$GREEN" "$@"
}

# Intended to be called as:
#   bash <(curl dl.google.com/.../build_ngx_pagespeed.sh) <args>

# If we set -e or -u then users of this script will see it silently exit on
# failure.  Instead we need to check the exit status of each command manually.
# The run function handles exit-status checking for system-changing commands.
# Additionally, this allows us to easily have a dryrun mode where we don't
# actually make any changes.
INITIAL_ENV=$(printenv | sort)
function run() {
  if "$DRYRUN"; then
    echo_color "$YELLOW" -n "would run"
    echo " $@"
    env_differences=$(comm -13 <(echo "$INITIAL_ENV") <(printenv | sort))
    if [ -n "$env_differences" ]; then
      echo "  with the following additional environment variables:"
      echo "$env_differences" | sed 's/^/    /'
    fi
  else
    if ! "$@"; then
      error "Failure running '$@', exiting."
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

function version_sort() {
  # We'd rather use sort -V, but that's not available on Centos 5.  This works
  # for versions in the form A.B.C.D or shorter, which is enough for our use.
  sort -t '.' -k 1,1 -k 2,2 -k 3,3 -k 4,4 -g
}

# Compare two numeric versions in the form "A.B.C".  Works with version numbers
# having up to four components, since that's enough to handle both nginx (3) and
# ngx_pagespeed (4).
function version_older_than() {
  local test_version="$1"
  local compare_to="$2"

  local older_version=$(echo $@ | tr ' ' '\n' | version_sort | head -n 1)
  test "$older_version" != "$compare_to"
}

function determine_latest_nginx_version() {
  # Scrape nginx's download page to try to find the most recent nginx version.

  nginx_download_url="https://nginx.org/en/download.html"
  function report_error() {
    fail "
Couldn't automatically determine the latest nginx version: failed to $@
$nginx_download_url"
  }

  nginx_download_page=$(curl -sS --fail "$nginx_download_url") || \
    report_error "download"

  download_refs=$(echo "$nginx_download_page" | \
    grep -o '/download/nginx-[0-9.]*[.]tar[.]gz') || \
    report_error "parse"

  versions_available=$(echo "$download_refs" | \
    sed -e 's~^/download/nginx-~~' -e 's~\.tar\.gz$~~') || \
    report_error "extract versions from"

  latest_version=$(echo "$versions_available" | version_sort | tail -n 1) || \
    report_error "determine latest version from"

  if version_older_than "$latest_version" "1.11.4"; then
    fail "
Expected the latest version of nginx to be at least 1.11.4 but found
$latest_version on $nginx_download_url"
  fi

  echo "$latest_version"
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
    status "Detected that we're missing the following depencencies:"
    echo "  $missing_dependencies"
    status "Installing them:"
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
  if "$ASSUME_YES"; then
    return
  fi

  local prompt="$1"
  echo_color "$YELLOW" -n "$prompt"
  read -p " [Y/n] " yn
  if [[ "$yn" == N* || "$yn" == n* ]]; then
    echo "Cancelled."
    exit 0
  fi
}

# If a string is very simple we don't need to quote it.  But we should quote
# everything else to be safe.
function needs_quoting() {
  echo "$@" | grep -q '[^a-zA-Z0-9./_=-]'
}

function escape_for_quotes() {
  echo "$@" | sed -e 's~\\~\\\\~g' -e "s~'~\\\\'~g"
}

function quote_arguments() {
  local argument_str=""
  for argument in "$@"; do
    if [ -n "$argument_str" ]; then
      argument_str+=" "
    fi
    if needs_quoting "$argument"; then
      argument="'$(escape_for_quotes "$argument")'"
    fi
    argument_str+="$argument"
  done
  echo "$argument_str"
}

function build_ngx_pagespeed() {
  getopt --test
  if [ "$?" != 4 ]; then
    # Even Centos 5 and Ubuntu 10 LTS have new-style getopt, so I don't expect
    # this to be hit in practice on systems that are actually able to run
    # PageSpeed.
    fail "Your version of getopt is too old.  Exiting with no changes made."
  fi

  opts=$(getopt -o v:n:mb:pslt:ya:dh \
    --longoptions ngx-pagespeed-version:,nginx-version:,dynamic-module \
    --longoptions buildir:,no-deps-check,psol-from-source,devel,build-type: \
    --longoptions assume-yes,additional-nginx-configure-arguments:,dryrun,help \
    -n "$(basename "$0")" -- "$@")
  if [ $? != 0 ]; then
    usage
    exit 1
  fi
  eval set -- "$opts"

  NPS_VERSION="DEFAULT"
  NGINX_VERSION=""
  BUILDDIR="$HOME"
  DO_DEPS_CHECK=true
  PSOL_FROM_SOURCE=false
  DEVEL=false
  BUILD_TYPE=""
  ASSUME_YES=false
  DRYRUN=false
  DYNAMIC_MODULE=false
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
      -m | --dynamic-module) shift
        DYNAMIC_MODULE=true
        ;;
      -b | --builddir) shift
        BUILDDIR="$1"
        shift
        ;;
      -p | --no-deps-check) shift
        DO_DEPS_CHECK=false
        ;;
      -s | --psol-from-source) shift
        PSOL_FROM_SOURCE=true
        ;;
      -l | --devel) shift
        DEVEL=true
        ;;
      -t | --build-type) shift
        BUILD_TYPE="$1"
        shift
        ;;
      -y | --assume-yes) shift
        ASSUME_YES="true"
        ;;
      -a | --additional-nginx-configure-arguments) shift
        ADDITIONAL_NGINX_CONFIGURE_ARGUMENTS="$1"
        shift
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

  USE_GIT_CHECKOUT="$DEVEL"
  ALREADY_CHECKED_OUT=false
  if [ -e PSOL_BINARY_URL ]; then
    status "Detected that we're running in an existing ngx_pagespeed checkout."
    USE_GIT_CHECKOUT=true
    ALREADY_CHECKED_OUT=true
  fi

  if "$ALREADY_CHECKED_OUT"; then
    if [ "$NPS_VERSION" != "DEFAULT" ]; then
      fail \
"The --ngx-pagespeed-version argument doesn't make sense when running within an existing checkout."
    fi
  elif [ "$NPS_VERSION" = "DEFAULT" ]; then
    if "$DEVEL"; then
      NPS_VERSION="master"
    else
      NPS_VERSION="latest-stable"
    fi
  fi

  if [ ! -d "$BUILDDIR" ]; then
    fail "Told to build in $BUILDDIR, but that directory doesn't exist."
  fi

  BUILD_NGINX=false
  if [ -n "$NGINX_VERSION" ]; then
    BUILD_NGINX=true
  fi

  if "$DEVEL"; then
    PSOL_FROM_SOURCE=true
    BUILD_NGINX=true
    if [ -n "$NGINX_VERSION" ]; then
      fail \
"The --devel argument conflicts with --nginx.  In devel mode we use the version of nginx that's included as a submodule."
    fi
    if "$DYNAMIC_MODULE"; then
      fail "Can't currently build a dynamic module in --devel mode."
    fi
  fi

  if "$PSOL_FROM_SOURCE" && [ -z "$BUILD_TYPE" ]; then
    if "$DEVEL"; then
      BUILD_TYPE="Debug"
    else
      BUILD_TYPE="Release"
    fi
  elif [ -n "$BUILD_TYPE" ]; then
    fail "Setting --build-type requires --psol-from-source or --devel."
  fi

  if [ "$NGINX_VERSION" = "latest" ]; then
    # When this function fails it prints the debugging information needed first
    # to stderr.
    NGINX_VERSION=$(determine_latest_nginx_version) || exit 1
  fi

  if "$DYNAMIC_MODULE"; then
    # Check that ngx_pagespeed and nginx are recent enough to support dynamic
    # modules.  Unfortunately NPS_VERSION might be a tag, in which case we don't
    # know.  If it's not a numeric version number, then assume it's recent
    # enough and if it's not they'll get an ugly compilation error later.
    # Luckily 1.10.33.5 was a while ago now.
    #
    # I'd like to use =~ here, but they changed syntax between v3 and v4 (quotes
    # moved from mandatory to optional to prohibited).
    if [[ "${NPS_VERSION#*[^0-9.]}" = "$NPS_VERSION" ]] &&
         version_older_than "$NPS_VERSION" "1.10.33.5"; then
      fail "
You're trying to build ngx_pagespeed $NPS_VERSION as a dynamic module, but
ngx_pagespeed didn't add support for dynamic modules until 1.10.33.5."
    fi

    if [ ! -z "NGINX_VERSION" ]; then
      if version_older_than "$NGINX_VERSION" "1.9.13"; then
        fail "
You're trying to build nginx $NGINX_VERSION as a dynamic module but nginx didn't
add support for dynamic modules in a way compatible with ngx_pagespeed until
1.9.13."
      fi
    fi
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

  extra_flags=()
  # Now make sure our dependencies are installed.
  if "$DO_DEPS_CHECK"; then
    if [ -f /etc/debian_version ]; then
      status "Detected debian-based distro."

      install_dependencies "apt-get install" debian_is_installed \
        build-essential zlib1g-dev libpcre3 libpcre3-dev unzip uuid-dev

      if gcc_too_old; then
        if [ ! -e /usr/lib/gcc-mozilla/bin/gcc ]; then
          status "Detected that gcc is older than 4.8.  Installing gcc-mozilla"
          status "which installs gcc-4.8 into /usr/lib/gcc-mozilla/ and doesn't"
          status "affect your global gcc installation."
          run sudo apt-get install gcc-mozilla
        fi

        extra_flags=("--with-cc=/usr/lib/gcc-mozilla/bin/gcc" \
                     "--with-ld-opt=-static-libstdc++")
      fi

    elif [ -f /etc/redhat-release ]; then
      status "Detected redhat-based distro."

      install_dependencies "yum install" redhat_is_installed \
        gcc-c++ pcre-devel zlib-devel make unzip wget libuuid-devel
      if gcc_too_old; then
        if [ ! -e /opt/rh/devtoolset-2/root/usr/bin/gcc ]; then
          redhat_major_version=$(
            cat /etc/redhat-release | grep -o -E '[0-9]+' | head -n 1)
          if [ "$redhat_major_version" == 5 ]; then
            slc_version=5
          elif [ "$redhat_major_version" == 6 ]; then
            slc_version=6
          else
            fail "
Unexpected major version $redhat_major_version in /etc/redhat-release:
$(cat /etc/redhat-release) Expected 5 or 6."
          fi

          status "Detected that gcc is older than 4.8.  Scientific Linux"
          status "provides a gcc package that installs gcc-4.8 into /opt/ and"
          status "doesn't affect your global gcc installation."
          slc_key="https://linux.web.cern.ch/linux/scientific6/docs/repository/"
          slc_key+="cern/slc6X/i386/RPM-GPG-KEY-cern"
          slc_key_out="$TEMPDIR/RPM-GPG-KEY-cern"
          run sudo wget "$slc_key" -O "$slc_key_out"
          run sudo rpm --import "$slc_key_out"

          repo_fname="/etc/yum.repos.d/slc${slc_version}-devtoolset.repo"
          if [ -e "$repo_fname" ]; then
            fail "Expected $repo_fname not to exist; aborting."
          fi

          repo_url="https://linux.web.cern.ch/linux/scientific${slc_version}/"
          repo_url+="/docs/repository/cern/devtoolset/"
          repo_url+="slc${slc_version}-devtoolset.repo"
          run sudo wget -O "$repo_fname" "$repo_url"
          run sudo yum install devtoolset-2-gcc-c++ devtoolset-2-binutils
        fi
        extra_flags=("--with-cc=/opt/rh/devtoolset-2/root/usr/bin/gcc")
      fi
    else
      fail "
This doesn't appear to be a deb-based distro or an rpm-based one.  Not going to
be able to install dependencies.  Please install dependencies manually and rerun
with --no-deps-check."
    fi
    status "Operating system dependencies are all set."
  else
    status "Not checking whether operating system dependencies are installed."
  fi

  function delete_if_already_exists() {
    if "$DRYRUN"; then return; fi

    local directory="$1"
    if [ -d "$directory" ]; then
      if [ ${#directory} -lt 8 ]; then
        fail "
Not deleting $directory; name is suspiciously short.  Something is wrong."
      fi

      continue_or_exit "OK to delete $directory?"
      run rm -rf "$directory"
    fi
  }

  # In general, the zip github builds for tag foo unzips to ngx_pagespeed-foo,
  # but it looks like they special case vVERSION tags to ngx_pagespeed-VERSION
  if [[ "$NPS_VERSION" =~ ^[0-9]*[.][0-9]*[.][0-9]*[.][0-9]*$ ]]; then
    # We've been given a numeric version number.  This has an associated tag
    # in the form vVERSION-beta.
    tag_name="v${NPS_VERSION}-beta"
    nps_downloaded_fname="ngx_pagespeed-${NPS_VERSION}-beta"
  else
    # We've been given a tag name, like latest-beta.  Download that directly.
    tag_name="$NPS_VERSION"
    nps_downloaded_fname="ngx_pagespeed-${NPS_VERSION}"
  fi

  install_dir="this-only-makes-sense-in-devel-mode"
  if "$USE_GIT_CHECKOUT"; then
    # We're either doing a --devel build, or someone is running us from an
    # existing git checkout.
    nps_module_dir="$PWD"
    install_dir="$nps_module_dir"
    if "$ALREADY_CHECKED_OUT"; then
      run cd "$nps_module_dir"
    else
      status "Checking out ngx_pagespeed..."
      run git clone "git@github.com:pagespeed/ngx_pagespeed.git" \
                    "$nps_module_dir"
      run cd "$nps_module_dir"
      run git checkout "$tag_name"
    fi
    submodules_dir="$nps_module_dir/testing-dependencies"
    if "$DEVEL"; then
      status "Downloading dependencies..."
      run git submodule update --init --recursive
      if [[ "$CONTINUOUS_INTEGRATION" != true ]]; then
        status "Switching submodules over to git protocol."
        # This lets us push to github by public key.
        for config in $(find .git/ -name config) ; do
          run sed -i s~https://github.com/~git@github.com:~ $config ;
        done
      fi
    fi
  else
    nps_baseurl="https://github.com/pagespeed/ngx_pagespeed/archive"
    nps_downloaded="$TEMPDIR/$nps_downloaded_fname.zip"
    status "Downloading ngx_pagespeed..."
    run wget "$nps_baseurl/$tag_name.zip" -O "$nps_downloaded"
    nps_module_dir="$BUILDDIR/$nps_downloaded_fname"
    delete_if_already_exists "$nps_module_dir"
    status "Extracting ngx_pagespeed..."
    run unzip -q "$nps_downloaded" -d "$BUILDDIR"
    run cd "$nps_module_dir"
  fi

  MOD_PAGESPEED_DIR=""
  PSOL_BINARY=""
  if "$PSOL_FROM_SOURCE"; then
    MOD_PAGESPEED_DIR="$PWD/testing-dependencies/mod_pagespeed"
    git submodule update --init --recursive -- "$MOD_PAGESPEED_DIR"
    run pushd "$MOD_PAGESPEED_DIR"

    if "$DEVEL"; then
      if [ ! -d "$HOME/apache2" ]; then
        run install/build_development_apache.sh 2.2 prefork
      fi
      cd devel
      run make apache_debug_psol
      PSOL_BINARY="$MOD_PAGESPEED_DIR/out/$BUILD_TYPE/pagespeed_automatic.a"
    else
      if "$DO_DEPS_CHECK"; then
        skip_deps_arg=""
      else
        skip_deps_arg="--skip_deps"
      fi

      run install/build_psol.sh --skip_tests --skip_packaging "$skip_deps_arg"
      PSOL_BINARY="$MOD_PAGESPEED_DIR/pagespeed/automatic/pagespeed_automatic.a"
    fi
    run popd
  else
    # Now we need to figure out what precompiled version of PSOL to build
    # ngx_pagespeed against.
    if "$DRYRUN"; then
      psol_url="https://psol.example.com/cant-get-psol-version-in-dry-run.tar.gz"
    elif [ -e PSOL_BINARY_URL ]; then
      # Releases after 1.11.33.4 there is a PSOL_BINARY_URL file that tells us
      # where to look.
      psol_url="$(scripts/format_binary_url.sh PSOL_BINARY_URL)"
      if [[ "$psol_url" != https://* ]]; then
        fail "Got bad psol binary location information: $psol_url"
      fi
    else
      # For past releases we have to grep it from the config file.  The url has
      # always looked like this, and the config file has contained it since
      # before we started tagging our ngx_pagespeed releases.
      psol_url="$(grep -o \
          "https://dl.google.com/dl/page-speed/psol/[0-9.]*.tar.gz" config)"
      if [ -z "$psol_url" ]; then
        fail "Couldn't find PSOL url in $PWD/config"
      fi
    fi

    status "Downloading PSOL binary..."
    run wget "$psol_url"

    status "Extracting PSOL..."
    run tar -xzf $(basename "$psol_url")  # extracts to psol/
  fi

  if "$DYNAMIC_MODULE"; then
    add_module="--add-dynamic-module=$nps_module_dir"
  else
    add_module="--add-module=$nps_module_dir"
  fi
  configure_args=("$add_module" "${extra_flags[@]}")

  if "$DEVEL"; then
    configure_args=("${configure_args[@]}"
                    "--prefix=$install_dir/nginx"
                    "--add-module=$submodules_dir/ngx_cache_purge"
                    "--add-module=$submodules_dir/ngx_devel_kit"
                    "--add-module=$submodules_dir/set-misc-nginx-module"
                    "--add-module=$submodules_dir/headers-more-nginx-module"
                    "--with-ipv6"
                    "--with-http_v2_module")
    if [ "$BUILD_TYPE" = "Debug" ]; then
      configure_args=("${configure_args[@]}" "--with-debug")
    fi
  fi

  echo
  if ! "$BUILD_NGINX"; then
    # Just prepare the module for them to install.
    status "ngx_pagespeed is ready to be built against nginx."
    echo "When running ./configure:"
    if "$PSOL_FROM_SOURCE"; then
      echo "  Set the following environment variables:"
      echo "    MOD_PAGESPEED_DIR=$MOD_PAGESPEED_DIR"
      echo "    PSOL_BINARY=$PSOL_BINARY"
    fi
    echo "  Give ./configure the following arguments:"
    echo "    $(quote_arguments "${configure_args[@]}")"
    echo
    if [ ${#extra_flags[@]} -eq 0 ]; then
      echo "If this is for integration with an already-built nginx, make sure"
      echo "to include any other arguments you originally passed to"
      echo "./configure.  You can see these with 'nginx -V'."
    else
      echo "Note: because we need to set $(quote_arguments "${extra_flags[@]}")"
      echo "on this platform, if you want to integrate ngx_pagespeed with an"
      echo "already-built nginx you're going to need to rebuild your nginx with"
      echo "those flags set."
    fi
  else
    if "$DEVEL"; then
      # Use the nginx we loaded as a submodule
      nginx_dir="$submodules_dir/nginx"
      configure_location="auto"
    else
      # Download and build the specified nginx version.
      nginx_leaf="nginx-${NGINX_VERSION}.tar.gz"
      nginx_fname="$TEMPDIR/$nginx_leaf"
      status "Downloading nginx..."
      run wget "http://nginx.org/download/$nginx_leaf" -O "$nginx_fname"
      nginx_dir="$BUILDDIR/nginx-${NGINX_VERSION}/"
      delete_if_already_exists "$nginx_dir"
      status "Extracting nginx..."
      run tar -xzf "$nginx_fname" --directory "$BUILDDIR"
      configure_location="."
    fi
    run cd "$nginx_dir"

    configure=("$configure_location/configure" "${configure_args[@]}")
    additional_configure_args=""
    if [ -z "${ADDITIONAL_NGINX_CONFIGURE_ARGUMENTS+x}" ]; then
      if ! "$ASSUME_YES"; then
        echo "About to build nginx.  Do you have any additional ./configure"
        echo "arguments you would like to set?  For example, if you would like"
        echo "to build nginx with https support give --with-http_ssl_module"
        echo "If you don't have any, just press enter."
        read -p "> " additional_configure_args
      fi
    else
      additional_configure_args="$ADDITIONAL_NGINX_CONFIGURE_ARGUMENTS"
    fi

    if [ -n "$additional_configure_args" ]; then
      # Split additional_configure_args respecting any internal quotation.
      # Otherwise things like --with-cc-opt='-foo -bar' won't work.
      eval additional_configure_args=("$additional_configure_args")
      configure=("${configure[@]}" "${additional_configure_args[@]}")
    fi

    echo "About to configure nginx with:"
    echo "   $(quote_arguments "${configure[@]}")"
    continue_or_exit "Does this look right?"
    MOD_PAGESPEED_DIR="$MOD_PAGESPEED_DIR" \
      PSOL_BINARY="$PSOL_BINARY" \
      run "${configure[@]}"

    if ! "$DEVEL"; then
      continue_or_exit "Build nginx?"
    fi
    run make

    if "$DEVEL"; then
      run make install

      status "Nginx installed with ngx_pagespeed, and set up for development."
      echo "To run tests:"
      echo "  cd $nps_module_dir"
      echo "  test/run_tests.sh"
      echo
      echo "To rebuild after changes:"
      echo "  scripts/rebuild.sh"
    else
      continue_or_exit "Install nginx?"
      run sudo make install

      echo
      if "$DYNAMIC_MODULE"; then
        echo "Nginx installed with ngx_pagespeed support available as a"
        echo "loadable module."
        echo
        echo "To load the ngx_pagespeed module, you'll need to add:"
        echo "  load_module \"modules/ngx_pagespeed.so\";"
        echo "at the top of your main nginx configuration file."
      else
        echo "Nginx installed with ngx_pagespeed support compiled-in."
      fi
      echo
      echo "If this is a new installation you probably need an init script to"
      echo "manage starting and stopping the nginx service.  See:"
      echo "  http://wiki.nginx.org/InitScripts"
      echo
      echo "You'll also need to configure ngx_pagespeed if you haven't yet:"
      echo "  https://developers.google.com/speed/pagespeed/module/configuration"
    fi
  fi
  if "$DRYRUN"; then
    echo_color "$YELLOW" "[this was a dry run; your system is unchanged]"
  fi
}

# Start running things from a call at the end so if this script is executed
# after a partial download it doesn't do anything.
build_ngx_pagespeed "$@"
