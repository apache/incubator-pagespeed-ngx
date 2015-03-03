import re

import config
import test_helpers as helpers



def test_quality_of_jpeg_output_images():
    url = "%s/image_rewriting/rewrite_images.html" % config.TEST_ROOT
    headers = {
        "PageSpeedFilters": "rewrite_images",
        "PageSpeedImageRecompressionQuality": "85",
        "PageSpeedJpegRecompressionQuality": "70"}

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

    # If this this test fails because the image size is 7673 bytes it means
    # that image_rewrite_filter.cc decided it was a good idea to convert to
    # progressive jpeg, and in this case it's not.  See the not above on
    # kJpegPixelToByteRatio.
    assert image_resp.getheader("content-type") == "image/jpeg"
    assert len(image_body) <= 7564   # resized
