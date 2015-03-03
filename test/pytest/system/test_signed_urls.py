import re

import pytest

import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER
combined_css = ".yellow{background-color:#ff0}"
url_regex = r'http:\/\/[^ ]+css\.pagespeed[^ ]+\.css'
url_path = "/mod_pagespeed_test/unauthorized/inline_css.html"
opts  = "?PageSpeedFilters=rewrite_images,rewrite_css"
page_url = "%s%s" % (url_path, opts)


@pytest.mark.skipif("not proxy")
class TestSignedUrls:
    def get_resource_url(self, headers):
        result, success = helpers.FetchUntil(
            page_url, headers = headers, proxy = proxy).waitFor(
            helpers.stringCountEquals, "all_styles.css.pagespeed.cf", 1)
        assert success, result.body
        _resp, body = result

        match = re.search(url_regex, body, re.MULTILINE)
        assert match
        resource_url = match.group(0)
        return resource_url

    # Signed Urls : Correct URL signature is passed
    def test_signature_is_passed(self):
        headers = {"Host": "signed-urls.example.com"}
        resource_url = self.get_resource_url(headers)

        result, success = helpers.FetchUntil(resource_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)

        assert success, result.body

    def test_incorrect_url_signature_is_passed(self):
        headers = {"Host": "signed-urls.example.com"}
        resource_url = self.get_resource_url(headers)
        # Replace valid signature with an invalid one
        invalid_url = "%sAAAAAAAAAA.css" % resource_url[:-14]
        resp, _body = helpers.fetch(invalid_url, headers = headers,
            proxy = proxy, allow_error_responses = True)
        assert resp.status == 404 or resp.status == 403

    def test_no_signature_is_passed(self):
        headers = {"Host": "signed-urls.example.com"}
        resource_url = self.get_resource_url(headers)
        # Remove signature
        invalid_url = "%s.css" % resource_url[:-14]
        resp, _body = helpers.fetch(invalid_url, headers = headers,
            proxy = proxy, allow_error_responses = True)
        assert resp.status == 404 or resp.status == 403

    def test_ignored_signature_correct_url_signature_is_passed(self):
        headers = {"Host": "signed-urls-transition.example.com"}
        resource_url = self.get_resource_url(headers)
        result, success = helpers.FetchUntil(resource_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)
        assert success, result.body


    def test_ignored_signature_incorrect_url_signature_is_passed(self):
        headers = {"Host": "signed-urls-transition.example.com"}
        resource_url = self.get_resource_url(headers)
        invalid_url = "%sAAAAAAAAAA.css" % resource_url[:-14]
        result, success = helpers.FetchUntil(invalid_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)
        assert success, result.body

    def test_ignored_signature_no_signature_is_passed(self):
        headers = {"Host": "signed-urls-transition.example.com"}
        resource_url = self.get_resource_url(headers)
        invalid_url = "%s.css" % resource_url[:-14]
        result, success = helpers.FetchUntil(invalid_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)
        assert success, result.body

    def test_unsigned_urls_ignored_signatures_bad_signature_is_passed(self):
        headers = {"Host": "unsigned-urls-transition.example.com"}
        resource_url = self.get_resource_url(headers)
        invalid_url = resource_url.replace("Cxc4pzojlP", "UH8L-zY4b4AAAAAAAAAA")
        result, success = helpers.FetchUntil(invalid_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)
        assert success, result.body

    def test_unsigned_urls_ignored_signatures_no_signature_is_passed(self):
        headers = {"Host": "unsigned-urls-transition.example.com"}
        resource_url = self.get_resource_url(headers)
        invalid_url = resource_url.replace("Cxc4pzojlP", "UH8L-zY4b4")
        result, success = helpers.FetchUntil(invalid_url, headers = headers,
            proxy = proxy).waitFor(helpers.stringCountEquals, combined_css, 1)
        assert success, result.body
