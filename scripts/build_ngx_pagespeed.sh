#!/bin/bash

function usage() {
  echo "
Usage: build_ngx_pagespeed.sh [options]

  Installs ngx_pagespeed and its dependencies.  Can optionally build and install
  nginx as well.

Options:
  -v, --ngx-pagespeed-version <ngx_pagespeed version>
      What version of ngx_pagespeed to build.  Valid options include:
      * latest-beta
      * latest-stable
      * a version number, such as 1.11.33.4

      If you don't specify a version, defaults to latest-stable.

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

  -d, --dryrun
      Don't make any changes to the system, just print what changes you
      would have made.

  -h, --help
      Print this message and exit."
}

# Prints an error message and exits with an error code.
function fail() {
  local error_message="$@"
  echo "$@" >&2

  # Normally I'd use $0 in "usage" here, but since most people will be running
  # this via curl, that wouldn't actually give something useful.
  echo >&2
  echo "For usage information, run this script with --help" >&2
  exit 1
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

  opts=$(getopt -o v:n:mb:pdh \
    --longoptions ngx-pagespeed-version:,nginx-version:,dynamic-module \
    --longoptions buildir:,no-deps-check,dryrun,help \
    -n "$(basename "$0")" -- "$@")
  if [ $? != 0 ]; then
    usage
    exit 1
  fi
  eval set -- "$opts"

  NPS_VERSION="latest-stable"
  NGINX_VERSION=""
  BUILDDIR="$HOME"
  DO_DEPS_CHECK=true
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

  if [ ! -d "$BUILDDIR" ]; then
    fail "Told to build in $BUILDDIR, but that directory doesn't exist."
  fi

  if [ "$NGINX_VERSION" = "latest" ]; then
    # When this function fails it prints the debugging information needed first
    # to stderr.
    NGINX_VERSION=$(determine_latest_nginx_version) || exit 1
  fi

  if "$DYNAMIC_MODULE"; then
    # Check that ngx_pagespeed and nginx are recent enough to support dynamic
    # modules.
    if version_older_than "$NPS_VERSION" "1.10.33.5"; then
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

        extra_flags=("--with-cc=/usr/lib/gcc-mozilla/bin/gcc" \
                     "--with-ld-opt=-static-libstdc++")
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
            fail "
Unexpected major version $redhat_major_version in /etc/redhat-release:
$(cat /etc/redhat-release) Expected 5 or 6."
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
    echo "Dependencies are all set."
  else
    echo "Not checking whether dependencies are installed."
  fi

  function delete_if_already_exists() {
    if "$DRYRUN"; then return; fi

    local directory="$1"
    if [ -d "$directory" ]; then
      if [ ${#directory} -lt 8 ]; then
        fail "
Not deleting $directory; name is suspiciously short.  Something is wrong."
        exit 1
      fi

      continue_or_exit "OK to delete $directory?"
      run rm -rf "$directory"
    fi
  }

  nps_baseurl="https://github.com/pagespeed/ngx_pagespeed/archive"
  # In general, the zip github builds for tag foo unzips to ngx_pagespeed-foo,
  # but it looks like they special case vVERSION tags to ngx_pagespeed-VERSION.
  if [[ "$NPS_VERSION" =~ ^[0-9]*[.][0-9]*[.][0-9]*[.][0-9]*$ ]]; then
    # We've been given a numeric version number.  This has an associated tag in
    # the form vVERSION-beta.
    nps_url_fname="v${NPS_VERSION}-beta"
    nps_downloaded_fname="ngx_pagespeed-${NPS_VERSION}-beta"
  else
    # We've been given a tag name, like latest-beta.  Download that directly.
    nps_url_fname="$NPS_VERSION"
    nps_downloaded_fname="ngx_pagespeed-${NPS_VERSION}"
  fi

  nps_downloaded="$TEMPDIR/$nps_downloaded_fname.zip"
  run wget "$nps_baseurl/$nps_url_fname.zip" -O "$nps_downloaded"
  nps_module_dir="$BUILDDIR/$nps_downloaded_fname"
  delete_if_already_exists "$nps_module_dir"
  echo "Extracting ngx_pagespeed..."
  run unzip -q "$nps_downloaded" -d "$BUILDDIR"
  run cd "$nps_module_dir"

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
    # always looked like this, and the config file has contained it since before
    # we started tagging our ngx_pagespeed releases.
    psol_url="$(
      grep -o "https://dl.google.com/dl/page-speed/psol/[0-9.]*.tar.gz" config)"
    if [ -z "$psol_url" ]; then
      fail "Couldn't find PSOL url in $PWD/config"
    fi
  fi

  run wget "$psol_url"
  echo "Extracting PSOL..."
  run tar -xzf $(basename "$psol_url")  # extracts to psol/

  if "$DYNAMIC_MODULE"; then
    add_module="--add-dynamic-module=$nps_module_dir"
  else
    add_module="--add-module=$nps_module_dir"
  fi
  configure_args=("$add_module" "${extra_flags[@]}")

  echo
  if [ -z "$NGINX_VERSION" ]; then
    # They didn't specify an nginx version, so we're just preparing the
    # module for them to install.
    echo "ngx_pagespeed is ready to be built against nginx."
    echo "When running ./configure pass in:"
    echo "  $(quote_arguments "${configure_args[@]}")"
    if [ ${#extra_flags[@]} -eq 0 ]; then
      echo "If this is for integration with an already-built nginx, make sure"
      echo "to include any other arguments you originally passed to ./configure"
      echo "You can see these with 'nginx -V'."
    else
      echo "Note: because we need to set $(quote_arguments "${extra_flags[@]}")"
      echo "on this platform, if you want to integrate ngx_pagespeed with an"
      echo "already-built nginx you're going to need to rebuild your nginx with"
      echo "those flags set."
    fi
  else
    # Download and build nginx.
    nginx_leaf="nginx-${NGINX_VERSION}.tar.gz"
    nginx_fname="$TEMPDIR/$nginx_leaf"
    run wget "http://nginx.org/download/$nginx_leaf" -O "$nginx_fname"
    nginx_dir="$BUILDDIR/nginx-${NGINX_VERSION}/"
    delete_if_already_exists "$nginx_dir"
    echo "Extracting nginx..."
    run tar -xzf "$nginx_fname" --directory "$BUILDDIR"
    "$DRYRUN" || cd "$nginx_dir"

    configure=("./configure" "${configure_args[@]}")
    echo "About to build nginx.  Do you have any additional ./configure"
    echo "arguments you would like to set?  For example, if you would like"
    echo "to build nginx with https support give --with-http_ssl_module"
    echo "If you don't have any, just press enter."
    read -p "> " additional_configure_args
    if [ -n "$additional_configure_args" ]; then
      # Split additional_configure_args respecting any internal quotation.
      # Otherwise things like --with-cc-opt='-foo -bar' won't work.
      eval additional_configure_args=("$additional_configure_args")
      configure=("${configure[@]}" "${additional_configure_args[@]}")
    fi
    echo "About to configure nginx with:"
    echo "   $(quote_arguments "${configure[@]}")"
    continue_or_exit "Does this look right?"
    run "${configure[@]}"

    continue_or_exit "Build nginx?"
    run make

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
  if "$DRYRUN"; then
    echo "[this was a dry run; your system is unchanged]"
  fi
}

# Start running things from a call at the end so if this script is executed
# after a partial download it doesn't do anything.
build_ngx_pagespeed "$@"
