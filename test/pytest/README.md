System tests in python (work in progress)

Installing prerequisites:

```bash
sudo pip install urllib3 pytest
# experimentally running tests in parallel
# sudo pip install pytest-xdist
```

Running the tests:

```bash
MPS_DIR=~/Code/google/ngxps-ttpr/mod_pagespeed \
NGINX_BINARY=~/Code/google/ngxps-ttpr/testing/sbin/nginx \
/home/oschaaf/Code/google/ngxps-ttpr/ngx_pagespeed/test/pytest/run.sh
```
