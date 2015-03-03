import re

import config
import test_helpers as helpers



def test_rewrite_images_inlines_compresses_and_resizes():
    url = ("%s/rewrite_images.html?PageSpeedFilters=rewrite_images" %
        config.EXAMPLE_ROOT)

    # Images inlined.
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "data:image/png", 1)
    assert success, result.body


    # Images rewritten.
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, ".pagespeed.ic", 2)
    assert success, result.body

    # Verify with a blocking fetch that pagespeed_no_transform worked and was
    # stripped.
    headers = {"X-PSA-Blocking-Rewrite": "psatest"}
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "images/disclosure_open_plus.png", 1)
    assert success, result.body

    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, '"pagespeed_no_transform"', 0)
    assert success, result.body


def test_size_of_rewritten_image():
    url = ("%s/rewrite_images.html?PageSpeedFilters=rewrite_images" %
        config.EXAMPLE_ROOT)
    # Note: We cannot do this above because the intervening FetchUntils will
    # clean up $OUTDIR.
    headers = {"Accept-Encoding": "gzip"}
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, ".pagespeed.ic", 2)
    assert success, result.body
    body = result.body
    results = re.findall(r'[^"]*.pagespeed.ic[^"]*', body)

    # If PreserveUrlRelativity is on, we need to find the relative URL and
    # absolutify it ourselves.
    results = [helpers.absolutify_url(url, u) for u in results]


    assert len(results) == 2
    _resp_img0, body_img0 = helpers.fetch(results[0])
    resp_img1, body_img1 = helpers.fetch(results[1])

    assert len(body_img0) < 25000  # re-encoded
    assert len(body_img1) < 24126  # resized

    # Make sure we have some valid headers.
    assert resp_img1.getheader("content-type").lower() == "image/jpeg"
    # Make sure the response was not gzipped.
    assert not resp_img1.getheader("content-encoding")
    # Make sure there is no vary-encoding
    assert not resp_img1.getheader("vary")
    # Make sure there is an etag
    assert resp_img1.getheader("etag").lower() in ['w/"0"', 'w/"0-gzip"']
    # Make sure an extra header is propagated from input resource to output
    # resource.  X-Extra-Header is added in debug.conf.template.
    assert resp_img1.getheader("x-extra-header")
    # Make sure there is a last-modified tag
    assert resp_img1.getheader("last-modified")
