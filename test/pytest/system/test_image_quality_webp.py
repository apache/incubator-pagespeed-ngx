import re

import config
import test_helpers as helpers


def quality_of_webp_output_images(headers):
    url = ("%s/webp_rewriting/rewrite_images.html?PageSpeedFilters="
        "convert_jpeg_to_webp,rewrite_images"
        "&PageSpeedImageRecompressionQuality=75"
        "&PageSpeedWebpRecompressionQuality=65"
        % config.TEST_ROOT)

    # 2 images optimized
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "256x192xPuzzle.jpg.pagespeed.ic", 1)
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
    assert image_resp.getheader("content-type") == "image/webp"
    assert len(image_body) <= 5140   # resized


def test_quality_of_webp_output_images_user_agent_header():
    quality_of_webp_output_images({"User-Agent": "webp"})

def test_quality_of_webp_output_images_accept_header():
    quality_of_webp_output_images({"Accept": "image/webp"})
