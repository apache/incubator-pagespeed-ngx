import re

import config
import test_helpers as helpers


def test_quality_of_jpeg_output_images_with_generic_quality_flag():
    url = "%s/image_rewriting/rewrite_images.html" % config.TEST_ROOT
    headers = {
        "PageSpeedFilters": "rewrite_images",
        "PageSpeedImageRecompressionQuality": "75"}
    # 2 images optimized
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, ".pagespeed.ic", 2)
    assert success, result.body
    body = result.body

    results = re.findall(r'[^"]*256x192.*Puzzle.*pagespeed.ic[^"]*', body)

    # Sanity check, we should only have one result
    assert len(results) == 1

    results = [helpers.absolutify_url(url, u) for u in results]
    image_resp, image_body = helpers.fetch(results[0], headers)

    # This filter produces different images on 32 vs 64 bit builds. On 32 bit,
    # the size is 8157B, while on 64 it is 8155B. Initial investigation showed
    # no visible differences between the generated images.
    # TODO(jmaessen) Verify that this behavior is expected.
    #
    # Note that if this test fails with 8251 it means that you have managed to
    # get progressive jpeg conversion turned on in this testcase, which makes
    # the output larger.  The threshold factor kJpegPixelToByteRatio in
    # image_rewrite_filter.cc is tuned to avoid that.
    assert image_resp.getheader("content-type") == "image/jpeg"
    assert len(image_body) <= 8157   # resized
