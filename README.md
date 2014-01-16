![ngx_pagespeed](https://lh6.googleusercontent.com/-qufedJIJq7Y/UXEvVYxyYvI/AAAAAAAADo8/JHDFQhs91_c/s401/04_ngx_pagespeed.png)

ngx_pagespeed speeds up your site and reduces page load time by automatically
applying web performance best practices to pages and associated assets (CSS,
JavaScript, images) without requiring you to modify your existing content or
workflow. Features include:

- Image optimization: stripping meta-data, dynamic resizing, recompression
- CSS & JavaScript minification, concatenation, inlining, and outlining
- Small resource inlining
- Deferring image and JavaScript loading
- HTML rewriting
- Cache lifetime extension
- and
  [more](https://developers.google.com/speed/docs/mod_pagespeed/config_filters)

To see ngx_pagespeed in action, with example pages for each of the
optimizations, see our <a href="http://ngxpagespeed.com">demonstration site</a>.

## How to build

Because nginx does not support dynamic loading of modules, you need to compile
nginx from source to add ngx_pagespeed. Alternatively, if you're using Tengine you can [install ngx_pagespeed without
recompiling Tengine](https://github.com/pagespeed/ngx_pagespeed/wiki/Using-ngx_pagespeed-with-Tengine).

1. Install dependencies:

   ```bash
   # These are for RedHat, CentOS, and Fedora.
   $ sudo yum install gcc-c++ pcre-dev pcre-devel zlib-devel make

   # These are for Debian. Ubuntu will be similar.
   $ sudo apt-get install build-essential zlib1g-dev libpcre3 libpcre3-dev
   ```

2. Download ngx_pagespeed:

   ```bash
   $ cd ~
   $ wget https://github.com/pagespeed/ngx_pagespeed/archive/v1.7.30.3-beta.zip
   $ unzip v1.7.30.3-beta.zip # or unzip v1.7.30.3-beta
   $ cd ngx_pagespeed-1.7.30.3-beta/
   $ wget https://dl.google.com/dl/page-speed/psol/1.7.30.3.tar.gz
   $ tar -xzvf 1.7.30.3.tar.gz # expands to psol/
   ```

3. Download and build nginx:

   ```bash
   $ # check http://nginx.org/en/download.html for the latest version
   $ wget http://nginx.org/download/nginx-1.4.4.tar.gz
   $ tar -xvzf nginx-1.4.4.tar.gz
   $ cd nginx-1.4.4/
   $ ./configure --add-module=$HOME/ngx_pagespeed-1.7.30.3-beta
   $ make
   $ sudo make install
   ```

If this doesn't work see the [build
troubleshooting](https://github.com/pagespeed/ngx_pagespeed/wiki/Build-Troubleshooting) page.

This will use a binary PageSpeed Optimization Library, but it's also possible to
[build PSOL from
source](https://github.com/pagespeed/ngx_pagespeed/wiki/Building-PSOL-From-Source).

Note: ngx_pagespeed currently doesn't support Windows or MacOS because the
underlying PSOL library doesn't.

## How to use

In your `nginx.conf`, add to the main or server block:

```nginx
pagespeed on;
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
location /ngx_pagespeed_global_statistics { allow 127.0.0.1; deny all; }
location /ngx_pagespeed_message { allow 127.0.0.1; deny all; }
location /pagespeed_console { allow 127.0.0.1; deny all; }
```

To confirm that the module is loaded, fetch a page and check that you see the
`X-Page-Speed` header:

```bash
$ curl -I 'http://localhost:8050/some_page/' | grep X-Page-Speed
X-Page-Speed: 1.7.30.3-...
```

Looking at the source of a few pages you should see various changes, such as
urls being replaced with new ones like `yellow.css.pagespeed.ce.lzJ8VcVi1l.css`.

For complete documentation, see [Using
PageSpeed](https://developers.google.com/speed/pagespeed/module/using).

There are extensive system tests which cover most of ngx_pagespeed's
functionality.  Consider [testing your
installation](https://github.com/pagespeed/ngx_pagespeed/wiki/Testing).

For feedback, questions, and to follow
the progress of the project:

- [ngx-pagespeed-discuss mailing
  list](https://groups.google.com/forum/#!forum/ngx-pagespeed-discuss)
- [ngx-pagespeed-announce mailing
  list](https://groups.google.com/forum/#!forum/ngx-pagespeed-announce)

Note: The
[canonicalize_javascript_libraries](https://developers.google.com/speed/pagespeed/module/filter-canonicalize-js)
depends on `pagespeed_libraries.conf` which is distributed in Apache's format.
To convert it to the Nginx format, run:

```bash
$ scripts/pagespeed_libraries_generator.sh > ~/pagespeed_libraries.conf
$ sudo mv ~/pagespeed_libraries.conf /etc/nginx/
```

And then include it in your Nginx configuration by reference:

```nginx
include pagespeed_libraries.conf;
pagespeed EnableFilters canonicalize_javascript_libraries;
```
