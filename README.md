# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is a work in progress**, and is not yet ready for production use
([current status](https://github.com/pagespeed/ngx_pagespeed/wiki/Status)). If
you are interested in test-driving the module, or contributing to the project,
please follow the instructions below. For feedback, questions, and to follow the
progress of the project:

- [ngx-pagespeed-discuss Google Group](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss)

---

The goal of ngx_pagespeed is to speed up your site and reduce page load time by
automatically applying web performance best practices to pages and associated
assets (CSS, JavaScript, images) without requiring that you modify your existing
content or workflow. Features will include:

- Image optimization: stripping meta-data, dynamic resizing, recompression
- CSS & JavaScript minification, concatenation, inlining, and outlining
- Small resource inlining
- Deferring image and JavaScript loading
- HTML rewriting
- Cache lifetime extension
- and [more](https://developers.google.com/speed/docs/mod_pagespeed/config_filters)


## How to build

nginx does not support dynamic loading of modules. You need to add ngx_pagespeed
as a build time dependency, and to do that you have to first build the pagespeed
optimization library.

First build mod_pagespeed against trunk, following these instructions through
the end of the "Compile" step, and making sure that when you run `gclient sync`
you run it against "trunk" and not "latest-beta":
https://developers.google.com/speed/docs/mod_pagespeed/build_from_source

Then build the pagespeed optimization library:

    $ cd ~/mod_pagespeed/src/net/instaweb/automatic
    $ make all

Then check out ngx_pagespeed:

    $ cd ~
    $ git clone https://github.com/pagespeed/ngx_pagespeed.git

Now download and build nginx:

    # check http://nginx.org/en/download.html for the latest version
    $ wget http://nginx.org/download/nginx-1.2.6.tar.gz
    $ tar -xvzf nginx-1.2.6.tar.gz
    $ cd nginx-1.2.6/src/
    $ ./configure --with-debug --add-module=$HOME/ngx_pagespeed
    $ make install

(This assumes you put everything in your home directory; if not, change paths
appropriately.  The only restriction is that the `mod_pagespeed` and
`ngx_pagespeed` directories need to have the same parent.)

## How to use

In your nginx.conf, add to the main or server block:

    pagespeed on;
    pagespeed_cache /path/to/cache/dir;
    error_log logs/error.log debug;

In every server block where pagespeed is enabled add:

    # This is a temporary workaround that ensures requests for pagespeed
    # optimized resources go to the pagespeed handler.
    location ~ "\.pagespeed\.[a-z]{2}\.[^.]{10}\.[^.]+" { }
    location ~ "^/ngx_pagespeed_static/" { }

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

    $ curl -s -D- 'http://localhost:8050/some_page/' | grep X-Page-Speed
    X-Page-Speed: 0.10.0.0

Looking at the source of a few pages you should see various changes, like urls
being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

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
      pagespeed Library 43 1o978_K0_L
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
      regression test with same filtered input twice in combination
      convert_meta_tags
    FAIL.

Each of these failed tests is a known issue:
 - [compression is enabled for rewritten JS.](
    https://github.com/pagespeed/ngx_pagespeed/issues/70)
   - If you're running a version of nginx without etag support (pre-1.3.3) you
     won't see this issue, which is fine.
 - [regression test with same filtered input twice in combination](
    https://github.com/pagespeed/ngx_pagespeed/issues/55)
 - [convert_meta_tags](https://github.com/pagespeed/ngx_pagespeed/issues/56)

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

## Configuration

Once configuration is complete, any mod_pagespeed configuration directive should
work in ngx_pagespeed after a small adjustment: replace '"ModPagespeed"' with
'"pagespeed "':

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
