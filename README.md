# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is alpha**. If you are interested in test-driving the module, or
contributing to the project, see below. For feedback, questions, and to follow
the progress of the project:

- [ngx-pagespeed-discuss Google Group](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss)

---

The goal of ngx_pagespeed is to speed up your site and reduce page load time by
automatically applying web performance best practices to pages and associated
assets (CSS, JavaScript, images) without requiring that you modify your existing
content or workflow. Features include:

- Image optimization: stripping meta-data, dynamic resizing, recompression
- CSS & JavaScript minification, concatenation, inlining, and outlining
- Small resource inlining
- Deferring image and JavaScript loading
- HTML rewriting
- Cache lifetime extension
- and [more](https://developers.google.com/speed/docs/mod_pagespeed/config_filters)
  - Note: not all mod_pagespeed features work in ngx_pagespeed yet.

To see ngx_pagespeed in action, with example pages for each of the
optimizations, see our <a href="http://ngxpagespeed.com">demonstration site</a>.

## How to build

Because nginx does not support dynamic loading of modules, you need to add
ngx_pagespeed as a build-time dependency.

### Simple method: Using a binary Pagespeed Optimization Library

Install dependencies:

    # These are for RedHat, CentOS, and Fedora.
    $ sudo yum install git gcc-c++ pcre-dev pcre-devel zlib-devel make

    # These are for Debian. Ubuntu will be similar.
    $ sudo apt-get install git-core build-essential zlib1g-dev libpcre3 libpcre3-dev

Check out ngx_pagespeed:

    $ cd ~
    $ git clone https://github.com/pagespeed/ngx_pagespeed.git

Download and build nginx:

    $ # check http://nginx.org/en/download.html for the latest version
    $ wget http://nginx.org/download/nginx-1.2.6.tar.gz
    $ tar -xvzf nginx-1.2.6.tar.gz
    $ cd nginx-1.2.6/
    $ ./configure --add-module=$HOME/ngx_pagespeed
    $ make install


If `make` fails with `unknown type name ‘off64_t’`,
add `--with-cc-opt='-DLINUX=2 -D_REENTRANT -D_LARGEFILE64_SOURCE -march=i686 -pthread'`
to `./configure` and try to `make` again.

If `configure` fails with `checking for psol ... not found` then open
`objs/autoconf.err` and search for `psol`.

If it's not clear what's wrong from
the error message, then send it to the [mailing
list](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss) and we'll
have a look at it.

### Complex method: Building the Pagespeed Optimization Library from source

First build mod_pagespeed against the current revision we work at:

    $ mkdir -p ~/bin
    $ cd ~/bin
    $ svn co http://src.chromium.org/svn/trunk/tools/depot_tools
    $ export PATH=$PATH:~/bin/depot_tools
    $ mkdir ~/mod_pagespeed
    $ cd ~/mod_pagespeed
    $ gclient config http://modpagespeed.googlecode.com/svn/trunk/src
    $ gclient sync --force --jobs=1
    $ cd src/
    $ svn up -r2748
    $ gclient runhooks
    $ make AR.host="$PWD/build/wrappers/ar.sh" \
           AR.target="$PWD/build/wrappers/ar.sh" \
           BUILDTYPE=Release \
           mod_pagespeed_test pagespeed_automatic_test

(See [mod_pagespeed: build from
source](https://developers.google.com/speed/docs/mod_pagespeed/build_from_source) if
you run into trouble, or ask for help on the mailing list.)

Then build the pagespeed optimization library:

    $ cd ~/mod_pagespeed/src/net/instaweb/automatic
    $ make AR.host="$PWD/../../../build/wrappers/ar.sh" \
           AR.target="$PWD/../../../build/wrappers/ar.sh" \
           all

While `make all` will always report an error, as long as it creates
`pagespeed_automatic.a` you have what you need.

Check out ngx_pagespeed:

    $ cd ~
    $ git clone https://github.com/pagespeed/ngx_pagespeed.git

Download and build nginx:

    $ # check http://nginx.org/en/download.html for the latest version
    $ wget http://nginx.org/download/nginx-1.2.6.tar.gz
    $ tar -xvzf nginx-1.2.6.tar.gz
    $ cd nginx-1.2.6/
    $ MOD_PAGESPEED_DIR="$HOME/mod_pagespeed/src" ./configure --add-module=$HOME/ngx_pagespeed
    $ make install

This assumes you put everything in your home directory; if not, change paths
appropriately.

For a debug build, remove the `BUILDTYPE=Release` option when running `make
mod_pagespeed_test pagespeed_automatic_test` and add the flag `--with-debug` to
`./configure --add-module=...`.

### Alternate method: Use Tengine

Tengine is an Nginx distribution that supports dynamically loaded modules.  You
can add ngx_pagespeed to an existing Tengine install without recompiling
Tengine.  First follow one of the two installation methods above until you get
to the "Download and build nginx" section.  Then run:

    # This might be /usr/local/tengine, depending on your configuration.
    $ cd /path/to/tengine/sbin/
    $ ./dso_tool --add-module=/path/to/ngx_pagespeed

This will prepare a dynamically loadable module out of ngx_pagespeed.  To check
that it worked you can verify that `/path/to/tengine/modules/` contains an
`ngx_pagespeed.so`.

You need to tell tengine to load this module.  Before continuing with "How to
use" below, add this to the top of your configuration:

    dso {
        load ngx_pagespeed.so;
    }

## How to use

In your `nginx.conf`, add to the main or server block:

    pagespeed on;

    # needs to exist and be writable by nginx
    pagespeed FileCachePath /var/ngx_pagespeed_cache;

In every server block where pagespeed is enabled add:

    # This is a temporary workaround that ensures requests for pagespeed
    # optimized resources go to the pagespeed handler.
    location ~ "\.pagespeed\.([a-z]\.)?[a-z]{2}\.[^.]{10}\.[^.]+" { }
    location ~ "^/ngx_pagespeed_static/" { }
    location ~ "^/ngx_pagespeed_beacon$" { }

If you're proxying, you need to strip off the `Accept-Encoding` header because
ngx_pagespeed does not (yet) handle compression from upstreams:

    proxy_set_header Accept-Encoding "";

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

    $ curl -s -D- 'http://localhost:8050/some_page/' | grep X-Page-Speed
    X-Page-Speed: 1.1.0.0

Looking at the source of a few pages you should see various changes, such as
urls being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

### Configuration Differences From mod_pagespeed

#### BeaconUrl

PageSpeed can use a beacon to track load times.  By default PageSpeed sends
beacons to `/ngx_pagespeed_beacon` on your site, but you can change this:

    pagespeed BeaconUrl /path/to/beacon;

If you do, you also need to change the regexp above from `location ~
"^/ngx_pagespeed_beacon$" { }` to `location ~ "^/path/to/beacon$" { }`.

As with <a
href="https://developers.google.com/speed/docs/mod_pagespeed/filter-instrumentation-add">ModPagespeedBeaconUrl</a>
you can set your beacons to go to another site by specifying a full path:

    pagespeed BeaconUrl http://thirdpartyanalytics.example.com/my/beacon;

### Testing

The generic Pagespeed system test is ported, and all but three tests pass.  To
run it you need to first build nginx.  You also need to check out mod_pagespeed,
but we can take a shortcut and do this the easy way, without gyp, because we
don't need any dependencies:

    $ svn checkout https://modpagespeed.googlecode.com/svn/trunk/ mod_pagespeed

Then run:

    test/nginx_system_test.sh \
      primary_port \
      secondary_port \
      mod_pagespeed_dir \
      nginx_executable_path

For example:

    $ test/nginx_system_test.sh 8050 8051 /path/to/mod_pagespeed \
        /path/to/sbin/nginx

This should print out a lot of lines like:

    TEST: Make sure 404s aren't rewritten
          check_not fgrep /mod_pagespeed_beacon /dev/fd/63

and then eventually:

    Failing Tests:
      compression is enabled for rewritten JS.
      convert_meta_tags
      insert_dns_prefetch
      insert_dns_prefetch
    FAIL.

Each of these failed tests is a known issue:
 - [compression is enabled for rewritten JS.](
    https://github.com/pagespeed/ngx_pagespeed/issues/70)
   - If you're running a version of nginx without etag support (pre-1.3.3) you
     won't see this issue, which is fine.
 - [convert_meta_tags](https://github.com/pagespeed/ngx_pagespeed/issues/56)
 - [insert_dns_prefetch](https://github.com/pagespeed/ngx_pagespeed/issues/114)

If it fails with some other error, that's a problem, and it would be helpful for
you to [submit a bug](https://github.com/pagespeed/ngx_pagespeed/issues/new).

#### Testing with memcached

Start an memcached server:

    memcached -p 11211

In `ngx_pagespeed/test/pagespeed_test.conf.template` uncomment:

    pagespeed MemcachedServers "localhost:11211";
    pagespeed MemcachedThreads 1;

Then run the system test as above.

#### Testing with valgrind

To run nginx as a single process, which is much easier to debug with valgrind,
put at the top of your config:

    daemon off;
    master_process off;

Then run nginx with valgrind:

    valgrind --leak-check=full /path/to/nginx/sbin/nginx

Because valgrind puts its results on stderr and there are a lot of them, it can
be useful to log that:

    ERR_FILE=~/tmp.ngx_pagespeed.valgrind.$(date +%s).err ; valgrind --leak-check=full /path/to/nginx/sbin/nginx 2> $ERR_FILE ; echo $ERR_FILE

## Configuration

Most mod_pagespeed configuration directives work in ngx_pagespeed after a small
adjustment: replace '"ModPagespeed"' with '"pagespeed "':

    mod_pagespeed.conf:
      ModPagespeedEnableFilters collapse_whitespace,add_instrumentation
      ModPagespeedRunExperiment on
      ModPagespeedExperimentSpec id=3;percent=50;default
      ModPagespeedExperimentSpec id=4;percent=50

    ngx_pagespeed.conf:
      pagespeed EnableFilters collapse_whitespace,add_instrumentation;
      pagespeed RunExperiment on;
      pagespeed ExperimentSpec "id=3;percent=50;default";
      pagespeed ExperimentSpec "id=4;percent=50";

## Preparing the binary distribution

If you just want to run ngx_pagespeed you don't need this.  This is
documentation on how the `psol/` directory was created and is maintained.

We redistribute precompiled libraries and the accompanying headers for the
pagespeed optimization library and its dependencies.  To update the headers,
run:

    $ cd ngx_pagespeed/
    $ scripts/copy_includes.sh /path/to/mod_pagespeed/src

This will delete `psol/include/` and recreate it from `mod_pagespeed/src` by
copying over all the headers and a few selected source files.  The commit diff
should only be the changes, but it can be huge.

To update the binaries, create a virtual machine running an old version of
Linux.  The current binaries were created on two CentOS 5.4 virtual machines,
32-bit and 64-bit.  Because the binaries will usually work on systems that are
more recent, it's important not to do this on your development machine.
Building the binaries meant building mod_pagespeed and pagespeed_automatic from
source, in separate directories with `BUILDTYPE=Release` on and off, and then
copying the resulting binaries over to `psol/lib/`:

    $ for buildtype in Debug Release ; do
        for arch in ia32 x64 ; do
          for library in
            net/instaweb/automatic/pagespeed_automatic.a
            out/Debug/obj.target/third_party/aprutil/libaprutil.a
            out/Debug/obj.target/third_party/apr/libapr.a
            out/Debug/obj.target/third_party/serf/libserf.a ; do
              scp machine-${arch}:mod_pagespeed_${buildtype}/src/${library}
                  psol/lib/${buildtype}/linux/${arch}/
          done
        done
      done
