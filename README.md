# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is alpha**, and is only ready for production use if you really
like to live on the edge. If you are interested in test-driving the module, or
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

    # These are for RedHat, CentOS, and Fedora.  Debian and Ubuntu will be
    # similar.
    $ sudo yum install git gcc-c++ pcre-dev pcre-devel zlib-devel make

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

If `configure` fails with `checking for psol ... not found` then open
`objs/autoconf.err` and search for `psol`.  If it's not clear what's wrong from
the error message, then send it to the [mailing
list](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss) and we'll
have a look at it.

### Complex method: Building the Pagespeed Optimization Library from source

First build mod_pagespeed against the current revision we work at:

    $ mkdir ~/mod_pagespeed
    $ cd ~/mod_pagespeed
    $ gclient config http://modpagespeed.googlecode.com/svn/trunk/src
    $ gclient sync --force --jobs=1
    $ cd src/
    $ svn up -r2338
    $ gclient runhooks
    $ make BUILDTYPE=Release mod_pagespeed_test pagespeed_automatic_test

(See [mod_pagespeed: build from
source](https://developers.google.com/speed/docs/mod_pagespeed/build_from_source) if
you run into trouble, or ask for help on the mailing list.)

Then build the pagespeed optimization library:

    $ cd ~/mod_pagespeed/src/net/instaweb/automatic
    $ make all

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

## How to use

In your `nginx.conf`, add to the main or server block:

    pagespeed on;
    pagespeed RewriteLevel CoreFilters;

    # needs to exist and be writable by nginx
    pagespeed FileCachePath /var/ngx_pagespeed_cache;

In every server block where pagespeed is enabled add:

    # This is a temporary workaround that ensures requests for pagespeed
    # optimized resources go to the pagespeed handler.
    location ~ "\.pagespeed\.[a-z]{2}\.[^.]{10}\.[^.]+" { }
    location ~ "^/ngx_pagespeed_static/" { }

If you're proxying, you need to strip off the `Accept-Encoding` header because
ngx_pagespeed does not (yet) handle compression from upstreams:

    proxy_set_header Accept-Encoding "";

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

    $ curl -s -D- 'http://localhost:8050/some_page/' | grep X-Page-Speed
    X-Page-Speed: 1.1.0.0

Looking at the source of a few pages you should see various changes, such as
urls being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

### Testing

The generic Pagespeed system test is ported, and all but three tests pass.  To
run it you need to first build and configure nginx.  Set it up something like:

    ...
    http {
      pagespeed on;

      // TODO(jefftk): this should be the default.
      pagespeed RewriteLevel CoreFilters;

      # This can be anywhere on your filesystem.
      pagespeed FileCachePath /path/to/ngx_pagespeed_cache;

      # For testing that the Library command works.
      pagespeed Library 43 1o978_K0_LNE5_ystNklf
                http://www.modpagespeed.com/rewrite_javascript.js;

      # These gzip options are needed for tests that assume that pagespeed
      # always enables gzip.  Which it does in apache, but not in nginx.
      gzip on;
      gzip_vary on;

      # Turn on gzip for all content types that should benefit from it.
      gzip_types application/ecmascript;
      gzip_types application/javascript;
      gzip_types application/json;
      gzip_types application/pdf;
      gzip_types application/postscript;
      gzip_types application/x-javascript;
      gzip_types image/svg+xml;
      gzip_types text/css;
      gzip_types text/csv;
      # "gzip_types text/html" is assumed.
      gzip_types text/javascript;
      gzip_types text/plain;
      gzip_types text/xml;

      gzip_http_version 1.0;

      ...

      server {
        listen 8050;
        server_name localhost;
        root /path/to/mod_pagespeed/src/install;
        index index.html;

        add_header Cache-Control "public, max-age=600";

        # Disable parsing if the size of the HTML exceeds 50kB.
        pagespeed MaxHtmlParseBytes 50000;

        location /mod_pagespeed_test/no_cache/ {
          add_header Cache-Control no-cache;
        }

        location /mod_pagespeed_test/compressed/ {
          add_header Cache-Control max-age=600;
          add_header Content-Encoding gzip;
          types {
            text/javascript custom_ext;
          }
        }

        ...
      }
    }

Then run the test, using the port you set up with `listen` in the configuration
file:

    /path/to/ngx_pagespeed/test/nginx_system_test.sh localhost:8050

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

    memcached -p 11213

To the configuration above add to the main or server block:

    pagespeed MemcachedServers "localhost:11213";
    pagespeed MemcachedThreads 1;

Then run the system test:

    /path/to/ngx_pagespeed/test/nginx_system_test.sh localhost:8050

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
