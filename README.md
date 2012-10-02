# ngx_pagespeed

This is the [nginx](http://nginx.org/) port of
[mod_pagespeed](https://developers.google.com/speed/pagespeed/).

## How to build

nginx does not support dynamic loading of modules. You need to add
ngx_pagespeed as a build time dependency, and to do that you have to first build
the pagespeed optimization library.

First build mod_pagespeed, following these instructions:
http://code.google.com/p/modpagespeed/wiki/HowToBuild

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

To your nginx.conf add to the main block or to a server or location block:

    pagespeed on;

Then fetch a page and note that it adds a comment:

    <!-- Processed through ngx_pagespeed using PSOL version 0.10.0.0  -->

That's all it does so far.