import pytest

import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER

@pytest.mark.skipif("not proxy")
class TestStickyOptionCookies:
    # Sticky option cookies: initially remove_comments only
    def test_initially_remove_comments_only(self):
        headers = {"Host": "options-by-cookies-enabled.example.com"}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count('<!-- This comment should not be deleted -->')
        assert body.count('  ') == 0
        assert body.count('Cookie') == 0
        assert ("%s" % resp.getheaders()).count("cookie") == 0

    # Sticky option cookies: wrong token has no effect
    def test_wrong_token_has_no_effect(self):
        headers = {"Host": "options-by-cookies-enabled.example.com"}
        page = ("/mod_pagespeed_test/forbidden.html"
                "?PageSpeedStickyQueryParameters=wrong_secret"
                "&PageSpeedFilters=+remove_comments"
            )
        url = "%s" % page
        resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count('<!-- This comment should not be deleted -->') == 0
        assert body.count('  ') == 0
        assert not resp.getheader("set-cookie")


    # Sticky option cookies: right token IS adhesive
    def test_right_token_is_adhesive(self):
        headers = {"Host": "options-by-cookies-enabled.example.com"}
        page = ("/mod_pagespeed_test/forbidden.html"
                "?PageSpeedStickyQueryParameters=sticky_secret"
                "&PageSpeedFilters=+remove_comments"
            )
        url = "%s" % page
        resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count('<!-- This comment should not be deleted -->') == 0
        assert body.count('  ') == 0
        assert resp.getheader("set-cookie").find(
            "PageSpeedFilters=%2bremove_comments;") == 0
        # We know we got the right cookie, now check that we got the right
        # number. Multiple values will be joined with "," - but we already have
        # on for the cookie date.
        assert resp.getheader("set-cookie").count(",") == 1

    # test_sticky_option_cookies_right_token_is_adhesive will test that we
    # receive a single cookie. Note that this helper works under that assumption
    def capture_cookie(self):
        headers = {"Host": "options-by-cookies-enabled.example.com"}
        page = ("/mod_pagespeed_test/forbidden.html"
                "?PageSpeedStickyQueryParameters=sticky_secret"
                "&PageSpeedFilters=+remove_comments"
            )
        url = "%s" % page
        result = helpers.fetch(url, headers = headers, proxy = proxy)
        return result.resp.getheader("set-cookie").split(";")[0]

    # Sticky option cookies: no token leaves option cookies untouched
    def test_no_token_leaves_option_cookies_untouched(self):
        # First, capture a cookie using a valid secret
        captured_cookie = self.capture_cookie()
        headers = {"Host": "options-by-cookies-enabled.example.com",
            "Cookie" : "%s" % captured_cookie}
        page = "/mod_pagespeed_test/forbidden.html"
        url = "%s" % page
        resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count('<!-- This comment should not be deleted -->') == 0
        assert body.count('  ') == 0
        assert not resp.getheader("set-cookie")


    # Sticky option cookies: wrong token expires option cookies
    def test_wrong_token_expires_option_cookies(self):
        # First, capture a cookie using a valid secret
        # self.captured_cookie = self.capture_cookie()
        # assert self.captured_cookie
        captured_cookie = self.capture_cookie()

        headers = {"Host": "options-by-cookies-enabled.example.com",
            "Cookie" : "%s" % captured_cookie}
        page = ("/mod_pagespeed_test/forbidden.html?"
                "PageSpeedStickyQueryParameters=off")
        url = "%s" % page
        resp, body = helpers.fetch(url, headers = headers, proxy = proxy)
        assert body.count('<!-- This comment should not be deleted -->') == 0
        assert body.count('  ') == 0
        assert resp.getheader("set-cookie").find(
            "PageSpeedFilters; Expires=Thu, 01 Jan 1970") == 0
        # Test no value is assigned to PageSpeedFilters in the set-cookie value
        assert resp.getheader("set-cookie").split(";")[0] == "PageSpeedFilters"

    # Sticky option cookies: back to remove_comments only
    def test_back_to_remove_comments_only(self):
        self.test_initially_remove_comments_only()
