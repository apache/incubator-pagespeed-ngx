# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/).

## How to build

nginx does not support dynamic loading of modules. You need to add
ngx_pagespeed as a build time dependency.

    $ cd /path/to/nginx
    $ auto/configure --add-module=/path/to/ngx_pagespeed
    $ make install

## How to use

TODO
