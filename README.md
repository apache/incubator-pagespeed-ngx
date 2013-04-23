![ngx_pagespeed](https://lh6.googleusercontent.com/-qufedJIJq7Y/UXEvVYxyYvI/AAAAAAAADo8/JHDFQhs91_c/s401/04_ngx_pagespeed.png)

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is alpha**. If you are interested in test-driving the module, or
contributing to the project, see below. For feedback, questions, and to follow
the progress of the project:

- [ngx-pagespeed-discuss mailing
  list](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss)
- [ngx-pagespeed-announce mailing
  list](https://groups.google.com/forum/#!forum/ngx-pagespeed-announce)

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
- and
  [more](https://developers.google.com/speed/docs/mod_pagespeed/config_filters)

Nearly all mod_pagespeed features work in ngx_pagespeed; see the ever-shrinking
[list of missing features](https://github.com/pagespeed/ngx_pagespeed/wiki/List-of-missing-mod_pagespeed-features).

To see ngx_pagespeed in action, with example pages for each of the
optimizations, see our <a href="http://ngxpagespeed.com">demonstration site</a>.

## How to build

Because nginx does not support dynamic loading of modules, you need to compile
nginx from source to add ngx_pagespeed. Alternatively, if you're using Tengine you can [install ngx_pagespeed without
recompiling Tengine](https://github.com/pagespeed/ngx_pagespeed/wiki/Using-ngx_pagespeed-with-Tengine).

1. Install dependencies:

   ```bash
   # These are for RedHat, CentOS, and Fedora.
   $ sudo yum install git gcc-c++ pcre-dev pcre-devel zlib-devel make

   # These are for Debian. Ubuntu will be similar.
   $ sudo apt-get install git-core build-essential zlib1g-dev libpcre3 libpcre3-dev
   ```

2. Check out ngx_pagespeed:

   ```bash
   $ cd ~
   $ git clone https://github.com/pagespeed/ngx_pagespeed.git
   ```

3. Download and build nginx:

   ```bash
   $ # check http://nginx.org/en/download.html for the latest version
   $ wget http://nginx.org/download/nginx-1.3.15.tar.gz
   $ tar -xvzf nginx-1.3.15.tar.gz
   $ cd nginx-1.3.15/
   $ ./configure --add-module=$HOME/ngx_pagespeed
   $ make install
   ```

If `make` fails with `unknown type name ‘off64_t’`,
add `--with-cc-opt='-DLINUX=2 -D_REENTRANT -D_LARGEFILE64_SOURCE -march=i686 -pthread'`
to `./configure` and try to `make` again.

If `configure` fails with `checking for psol ... not found` then open
`objs/autoconf.err` and search for `psol`.

If it's not clear what's wrong from
the error message, then send it to the [mailing
list](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss) and we'll
have a look at it.

This will use a binary PageSpeed Optimization Library.  If you would rather
build PSOL from source, [here's how to do that](https://github.com/pagespeed/ngx_pagespeed/wiki/Building-PSOL-From-Source).

## How to use

In your `nginx.conf`, add to the main or server block:

```nginx
pagespeed on;

# needs to exist and be writable by nginx
pagespeed FileCachePath /var/ngx_pagespeed_cache;
```

In every server block where pagespeed is enabled add:

```apache
#  Ensure requests for pagespeed optimized resources go to the pagespeed
#  handler and no extraneous headers get set.
location ~ "\.pagespeed\.([a-z]\.)?[a-z]{2}\.[^.]{10}\.[^.]+" { add_header "" ""; }
location ~ "^/ngx_pagespeed_static/" { }
location ~ "^/ngx_pagespeed_beacon$" { }
location /ngx_pagespeed_statistics { allow 127.0.0.1; deny all; }
location /ngx_pagespeed_message { allow 127.0.0.1; deny all; }
```

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

```bash
$ curl -s -D- 'http://localhost:8050/some_page/' | grep X-Page-Speed
X-Page-Speed: 1.4.0.0-2729
```

Looking at the source of a few pages you should see various changes, such as
urls being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

When reading the [mod_pagespeed
documentation](https://developers.google.com/speed/docs/mod_pagespeed/using_mod),
keep in mind that you need to make a small adjustment to configuration
directives: replace **ModPagespeed** with **pagespeed**:

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

For more configuration details, see the [differences from mod_pagespeed
configuration](https://github.com/pagespeed/ngx_pagespeed/wiki/Configuration-differences-from-mod_pagespeed)
wiki page.

There are extensive system tests which cover most of ngx_pagespeed's
functionality.  Consider [testing your
installation](https://github.com/pagespeed/ngx_pagespeed/wiki/Testing).