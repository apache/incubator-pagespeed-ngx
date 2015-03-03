import re

import config
import test_helpers as helpers

def test_pagespeed_qs_param_insert_canonical_href_link():
    url = "%s/defer_javascript.html?PageSpeed=noscript" % config.EXAMPLE_ROOT
    assert helpers.fetch(url).body.count(
        'link rel="canonical" href="%s%s/defer_javascript.html"' %
        (config.PRIMARY_SERVER, config.EXAMPLE_ROOT))

# Checks that defer_javascript injects 'pagespeed.deferJs' from defer_js.js,
# but strips the comments.
def test_defer_javascript_optimize_mode():
    url = ("%s/defer_javascript.html?PageSpeedFilters=defer_javascript" %
        config.EXAMPLE_ROOT)

    _resp, body = helpers.fetch(url)
    assert body.count("text/psajs")
    assert body.count("js_defer")
    assert body.count("PageSpeed=noscript")


# Checks that defer_javascript,debug injects 'pagespeed.deferJs' from
# defer_js.js, but retains the comments.
def test_defer_javascript_debug_optimize_mode():
    url = ("%s/defer_javascript.html?PageSpeedFilters=defer_javascript,debug" %
        config.EXAMPLE_ROOT)

    _resp, body = helpers.fetch(url)
    assert body.count("text/psajs")
    assert body.count("js_defer_debug")
    assert body.count("PageSpeed=noscript")

    # The deferjs src url is in the format js_defer.<hash>.js. This strips out
    # everthing except the js filename and saves it to test fetching later.
    match = re.search(r'src="/.*/(js_defer.*\.js)"', body)
    assert match
    js_defer_leaf = match.group(1)
    assert js_defer_leaf

    js_defer_url = "http://%s/%s/%s" % (
        config.PROXY_DOMAIN, config.PSA_JS_LIBRARY_URL_PREFIX,
        js_defer_leaf)
    resp, _body = helpers.fetch(js_defer_url)
    assert resp.getheader("cache-control") == "max-age=31536000"


# Checks that we return 404 for static file request without hash.
def test_access_to_js_defer_js_without_hash_returns_404():
    url = ("http://%s/%s/js_defer.js" %
        (config.PROXY_DOMAIN, config.PSA_JS_LIBRARY_URL_PREFIX))
    assert helpers.fetch(url, allow_error_responses = True).resp.status == 404

# Checks that outlined js_defer.js is served correctly.


def test_serve_js_defer_0_js():
    url = ("http://%s/%s/js_defer.0.js" %
        (config.PROXY_DOMAIN, config.PSA_JS_LIBRARY_URL_PREFIX))
    resp, _body = helpers.fetch(url)
    assert resp.getheader("cache-control") == "max-age=300,private"


# Checks that outlined js_defer_debug.js is  served correctly.
def test_serve_js_defer_debug_0_js():
    url = ("http://%s/%s/js_defer_debug.0.js" %
        (config.PROXY_DOMAIN, config.PSA_JS_LIBRARY_URL_PREFIX))
    resp, _body = helpers.fetch(url)
    assert resp.getheader("cache-control") == "max-age=300,private"
