import config
import test_helpers as helpers


############################### tests #########################################

def test_initial_header():
    resp, _body = helpers.fetch(
        "%s/combine_css.html" %
        config.EXAMPLE_ROOT)

    print "Checking for X-Mod-Pagespeed header"
    ps_server_header = resp.getheader("x-mod-pagespeed")
    if ps_server_header is None:
        ps_server_header = resp.getheader("x-page-speed")

    assert (ps_server_header is not None)

    print "Checking that we don't have duplicate X-Mod-Pagespeed headers"
    assert (',' not in ps_server_header)

    print "Checking that we don't have duplicate headers"
    responseHeaders = resp.getheaders()
    assert(len(set(responseHeaders)) == len(responseHeaders))

    print "Checking for lack of E-tag"
    assert(not resp.getheader("etag"))

    print "Checking for presence of Vary."
    assert(resp.getheader("vary") == "Accept-Encoding")

    print "Checking for absence of Last-Modified"
    assert(not resp.getheader("last-modified"))

    # Note: This is in flux, we can now allow cacheable HTML and this test will
    # need to be updated if this is turned on by default.
    print "Checking for presence of Cache-Control: max-age=0, no-cache"
    assert resp.getheader("cache-control") == "max-age=0, no-cache"

    print "Checking for absence of X-Frame-Options: SAMEORIGIN"
    assert(not resp.getheader("x-frame-options"))


def test_pagespeed_added_with_pagespeed_on():
    print "X-Mod-Pagespeed header added when PageSpeed=on"
    resp, _body = helpers.fetch(
        "%s/combine_css.html?PageSpeed=on" %
        config.EXAMPLE_ROOT)
    assert(resp.getheader("x-mod-pagespeed")
           or resp.getheader("x-page-speed"))


def test_pagespeed_not_added_with_pagespeed_off():
    print "X-Mod-Pagespeed header not added when PageSpeed=off"
    resp, _body = helpers.fetch(
        "%s/combine_css.html?PageSpeed=off" %
        config.EXAMPLE_ROOT)
    assert(not resp.getheader("x-mod-pagespeed")
           and not resp.getheader("x-page-speed"))
