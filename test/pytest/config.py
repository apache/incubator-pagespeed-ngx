from copy import copy
import logging
import os

import pytest

############################### test args      ################################
TEST_TMP_DIR = "/home/oschaaf/Code/google/ngxps-ttpr/ngx_pagespeed/test/tmp"
PRIMARY_SERVER = "http://localhost:8050"
SECONDARY_SERVER = "http://localhost:8051"


EXAMPLE_ROOT = "/mod_pagespeed_example"
TEST_ROOT = "/mod_pagespeed_test"
DEFAULT_USER_AGENT = ("Mozilla/5.0 (X11; U; Linux x86_64; en-US) "
    "AppleWebKit/534.0 (KHTML, like Gecko) Chrome/6.0.408.1 Safari/534.0")
# TODO(oschaaf): check
HTTPS_HOST = ""  # SECONDARY_HOST
HTTPS_EXAMPLE_ROOT = "http://%s/mod_pagespeed_example" % HTTPS_HOST

# TODO(oschaaf): seems almost unused at first glance:
PROXY_DOMAIN = "localhost:8050"
PSA_JS_LIBRARY_URL_PREFIX = "pagespeed_custom_static"

# TODO(oschaaf): seems almost unused at first glance:
REWRITTEN_ROOT = EXAMPLE_ROOT
REWRITTEN_TEST_ROOT = TEST_ROOT

# Test will dump its log(s) here:
LOG_ROOT=TEST_TMP_DIR
DISABLE_FONT_API_TESTS = False

FETCH_ON_START_AND_END = False


logging.basicConfig(
    format = '%(asctime)s %(message)s',
    filename = '%s/pytest_psol.log' % TEST_TMP_DIR,
    level = logging.DEBUG,
    filemode = "w")

log = logging.getLogger("psol")

# Override all the module variables above from the environment (if specified)
# TODO(oschaaf): only allow overriding things that make sense
candidates = copy(globals())
for name in candidates:
    if name.startswith("_"):
        continue
    if name in os.environ:
        globals()[name] = os.getenv(name)
        log.debug("var: %s:%s (from env)" % (name, globals()[name]))
        print "%s:%s (from env)" % (name, globals()[name])
    else:
        log.debug("var: %s:%s (default)" % (name, globals()[name]))
        print "%s:%s (default)" % (name, globals()[name])



