import config
import test_helpers as helpers

# Test DNS prefetching. DNS prefetching is dependent on user agent, but is
# enabled for Wget UAs, allowing this test to work with our default wget params.
def test_dedup_inlined_images_inline_images():
    filter_name = "dedup_inlined_images,inline_images"
    url = "%s/dedup_inlined_images.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)
    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "inlineImg(", 4)

    assert success, result.body
    assert result.body.count("PageSpeed=noscript")
