import re

import config
import test_helpers as helpers

# TODO(oschaaf): fix https test / setup vhost once this lands in ngx_pagespeed
def test_simple_test_that_https_is_working():
    if config.HTTPS_HOST:
        assert False
