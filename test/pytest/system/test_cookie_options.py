import pytest

import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER

@pytest.mark.skipif("not proxy")
class TestCookieOptions:
    # Cookie options on: by default comments not removed, whitespace is
    def test_setting_cookie_on_options_no_cookie(self):
        headers = {"Host": "options-by-cookies-enabled.example.com"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        _resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count("<!--")
        assert body.count("  ") == 0


    # Cookie options on: set option by cookie takes effect
    def test_setting_cookie_on_options_takes_effect(self):
        headers = {"Host": "options-by-cookies-enabled.example.com",
            "Cookie" : "PageSpeedFilters=%2bremove_comments"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        _resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count("<!--") == 0
        assert body.count("  ") == 0

    # Cookie options on: set option by cookie takes effect
    def test_setting_cookie_on_options_invalid_cookie_takes_no_effect(self):
        # The '+' must be encoded as %2b for the cookie parsing code to accept
        # it.
        headers = {"Host": "options-by-cookies-enabled.example.com",
            "Cookie" : "PageSpeedFilters=+remove_comments"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        _resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count("<!--")
        assert body.count("  ") == 0


    # Cookie options off: by default comments nor whitespace removed
    def test_setting_cookie_on_options_no_cookie(self):
        headers = {"Host": "options-by-cookies-disabled.example.com"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        _resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count("<!--")
        assert body.count("  ")

    # Cookie options off: set option by cookie has no effect
    def test_setting_cookie_on_options_cookie_no_effect(self):
        headers = {"Host": "options-by-cookies-disabled.example.com",
            "Cookie" : "PageSpeedFilters=%2bremove_comments"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        _resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count("<!--")
        assert body.count("  ")
