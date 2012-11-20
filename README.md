# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is a work in progress**, and is not yet functional ([current
status](https://github.com/pagespeed/ngx_pagespeed/wiki/Status)). If you are
interested in test-driving the module, or contributing to the project, please
follow the instructions below. For feedback, questions, and to follow the
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
the end of the "Compile" step:
https://developers.google.com/speed/docs/mod_pagespeed/build_from_source

When you run `gclient sync`, be sure to run it against
http://modpagespeed.googlecode.com/svn/trunk/src

Then build the pagespeed optimization library:

    $ cd /where/you/built/mod_pagespeed/src/net/instaweb/automatic
    $ make all

Then move the mod_pagespeed directory to a parallel directory to your
ngx_pagespeed checkout:

    $ cd /path/to/ngx_pagespeed
    $ mv /where/you/built/mod_pagespeed /path/to/mod_pagespeed

Now build nginx:

    $ cd /path/to/nginx
    $ auto/configure --with-debug --add-module=/path/to/ngx_pagespeed
    $ make install

While ngx_pagespeed doesn't need to be anywhere specific in relation to nginx,
the mod_pagespeed directory and the ngx_pagespeed directory must have the same
parent.

## How to use

In your nginx.conf, add to the main or server block:

    pagespeed on;
    pagespeed_cache /path/to/cache/dir;
    error_log logs/error.log debug;

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

    $ curl -s -D- 'http://localhost:8050/some_page/' | grep X-Page-Speed
    X-Page-Speed: 0.10.0.0

Looking at the source of a few pages you should see various changes, like urls
being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

### Testing

The generic Pagespeed system test is ported, but doesn't pass yet.  To run it
you need to first build and configure nginx.  Set it up something like:

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

        location /mod_pagespeed_test/no_cache/ {
          add_header Cache-Control no-cache;
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

    FAIL.

along with a failing test because ngx_pagespeed is not yet complete.

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
