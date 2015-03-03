import config
import test_helpers as helpers


headers = {"User-Agent" : "iPhone"}

# Checks that inline_preview_images injects compiled javascript
def test_inline_preview_images_optimize_mode():
    filter_name = "inline_preview_images"
    url = "%s/delay_images.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)

    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "pagespeed.delayImagesInit", 1)
    assert success, result.body
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "/*", 0)
    assert success, result.body

# Checks that inline_preview_images,debug injects from javascript
# in non-compiled mode
def test_inline_preview_images_debug_mode():
    filter_name = "inline_preview_images,debug"
    url = "%s/delay_images.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)
    result, success = helpers.FetchUntil(url, headers = headers).waitFor(
        helpers.stringCountEquals, "pagespeed.delayImagesInit", 4)
    assert success, result.body
