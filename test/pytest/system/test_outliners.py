import re

import config
import test_helpers as helpers




def test_outline_css_outlines_large_styles_but_not_small_ones():
    url = ("%s/outline_css.html?PageSpeedFilters=outline_css" %
        config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    # outlined
    assert len(re.findall(r'<link.*text/css.*large">', body)) == 1
    # not outlined
    assert len(re.findall(r'<style.*small', body)) == 1


def test_outline_javascript_outlines_large_scripts_but_not_small_ones():
    url = ("%s/outline_javascript.html?PageSpeedFilters=outline_javascript" %
        config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)

    # outlined
    assert len(re.findall(r'<script.*large.*src=', body)) == 1
    # not outlined
    assert len(re.findall(r'<script.*small.*var hello', body)) == 1


def test_compression_is_enabled_for_rewritten_js():
    url = ("%s/outline_javascript.html?PageSpeedFilters=outline_javascript" %
        config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)

    results = re.findall(r'http://.*.pagespeed.*.js', body)
    assert len(results) == 1
    js_url = results[0]
    print "js_url: %s" % js_url

    js_resp, _js_body = helpers.fetch(js_url, {"Accept-Encoding": "gzip"})
    assert js_resp.getheader("content-encoding") == "gzip"
    # check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
    assert js_resp.getheader("etag") in ['W/"0"', 'W/"0-gzip"']
    assert js_resp.getheader("last-modified")
