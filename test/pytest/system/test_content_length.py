import re

import config
import test_helpers as helpers


def test_pagespeed_resources_should_have_a_content_length():
    url = ("%s/rewrite_css_images.html?PageSpeedFilters=rewrite_css" %
        config.EXAMPLE_ROOT)

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "rewrite_css_images.css.pagespeed.cf", 1)
    assert success, result.body

    # Pull the rewritten resource name out so we get an accurate hash.
    matches = re.findall(r'[^"]*rewrite_css_images.css.pagespeed.cf[^"]*',
      result.body)
    # Sanity check, we should only have one result
    assert len(matches) == 1

    # TODO(oschaaf): rewrite domain proxy? see original code.
    # This will use REWRITE_DOMAIN as an http_proxy if set, otherwise no proxy.
    result = helpers.fetch(helpers.absolutify_url(url, matches[0]))
    assert result.resp.getheader("content-length")
    assert not result.resp.getheader("transfer-encoding")
    assert result.resp.getheader("cache-control")
    assert result.resp.getheader("cache-control").count("private") == 0



