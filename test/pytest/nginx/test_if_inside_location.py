import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER

def test_if_inside_location():
    url = ("http://if-in-location.example.com/"
            "mod_pagespeed_example/inline_javascript.html")
    headers = {"X-Custom-Header-Inline-Js" :"Yes"}
    # When we specify the X-Custom-Header-Inline-Js that triggers an if block in
    # the config which turns on inline_javascript.
    result, success = helpers.FetchUntil(
        url, headers = headers, proxy = proxy).waitFor(
        helpers.stringCountEquals, "document.write", 1)
    assert success, result.body
    result = helpers.fetch(
        url, headers = headers, proxy = proxy)
    assert result.resp.getheader("x-inline-javascript") == "Yes"
    assert result.body.count("inline_javascript.js") == 0

    # Without that custom header we don't trigger the if block, and shouldn't
    # get any inline javascript.
    result = helpers.fetch(url, proxy = proxy)
    assert result.resp.getheader("x-inline-javascript") == "No"
    assert result.body.count("inline_javascript.js")
    assert result.body.count("document.write") == 0
