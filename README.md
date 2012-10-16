# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/mod).

**ngx_pagespeed is a work in progress**, and is not yet functional. If you are
interested in test-driving the module, or contributing to the project, please
follow the instructions below. For feedback, questions, and to follow the progress of the project:

- [ngx-pagespeed-discuss Google Group](https://groups.google.com/d/topic/ngx-pagespeed-discuss/)

---

ngx_pagespeed speeds up your site and reduces page load time by automatically
applying web performance best practices to pages, and associated assets (CSS,
JavaScript, images) without requiring that you modify your existing content or
workflow. Features (will) include:

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

First build mod_pagespeed, following these instructions:
https://developers.google.com/speed/docs/mod_pagespeed/build_from_source

Then build the pagespeed optimization library:

    $ cd /where/you/built/mod_pagespeed/src/net/instaweb/automatic
    $ make all

Then move the mod_pagespeed directory to a parallel directory to your
ngx_pagespeed checkout:

    $ cd /path/to/ngx_pagespeed
    $ mv /where/you/built/mod_pagespeed /path/to/mod_pagespeed

Now build nginx:

    $ cd /path/to/nginx
    $ auto/configure --add-module=/path/to/ngx_pagespeed
    $ make install

While ngx_pagespeed doesn't need to be anywhere specific in relation to nginx,
the mod_pagespeed directory and the ngx_pagespeed directory must have the same
parent.

## How to use

In your nginx.conf, add to the main or server block:

    pagespeed on;
    pagespeed_cache /path/to/cache/dir;

To confirm that the module is loaded, fetch a page and check that you see the
following comment in the source:

    <!-- Processed through ngx_pagespeed using PSOL version 0.10.0.0 -->

### Testing

There is an example html file in:

    test/www/test.html

If you fetch it through nginx with ngx_pagespeed enabled you should see it
rewritten to look like the html in:

    test/expected/test.html
