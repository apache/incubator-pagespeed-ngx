import config
import test_helpers as helpers


# This tests whether fetching "/" gets you "/index.html".  With async
# rewriting, it is not deterministic whether inline css gets
# rewritten.  That's not what this is trying to test, so we use
# ?PageSpeed=off.

def test_in_place_resource_optimization():
    # Note: we intentionally want to use an image which will not appear on
    # any HTML pages, and thus will not be in cache before this test is run.
    # (Since the system_test is run multiple times without clearing the cache
    # it may be in cache on some of those runs, but we know that it was put in
    # the cache by previous runs of this specific test.)
    url = "%s/ipro/test_image_dont_reuse.png" % config.TEST_ROOT

    # Size between original image size and rewritten image size (in bytes).
    # Used to figure out whether the returned image was rewritten or not.
    threshold_size = 13000

    # Check that we compress the image (with IPRO).
    # Note: This requests $URL until it's size is less than $THRESHOLD_SIZE.
    headers = {"X-PSA-Blocking-Rewrite": "psatest"}

    result, success = helpers.FetchUntil(url, headers = headers
        ).waitFor(lambda r: len(r.body) < threshold_size)
    assert success, result.body

    # TODO(oschaaf): The original tests also looks on the disk to check the
    # fetched file. What does that add?

    # Check that resource is served with small Cache-Control header (since
    # we cannot cache-extend resources served under the original URL).
    cc = result.resp.getheader("cache-control")
    assert cc
    int_cc = int(cc.replace("max-age=", ""))
    assert int_cc < 1000

    # Check that the original image is greater than threshold to begin with.
    _resp, body = helpers.fetch("%s?PageSpeed=off" % url)
    assert len(body) > threshold_size
