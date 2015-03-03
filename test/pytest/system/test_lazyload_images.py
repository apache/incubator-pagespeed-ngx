import re

import config
import test_helpers as helpers

def test_lazyload_images():
    url = ("%s/lazyload_images.html?PageSpeedFilters=lazyload_images" %
        config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)

    assert body.count("pagespeed_lazy_src=\"images/Puzzle.jpg\"")
    assert body.count("pagespeed.lazyLoadInit")  # inline script injected


def test_lazyload_images_optimize_mode():
    url = ("%s/lazyload_images.html?PageSpeedFilters=lazyload_images" %
        config.EXAMPLE_ROOT)
    resp, body = helpers.fetch(url)
    assert body.count("pagespeed.lazyLoad")
    assert not re.search('/\*', body)
    assert body.count("PageSpeed=noscript")

    # The lazyload placeholder image is in the format 1.<hash>.gif. This matches
    # the first src attribute set to the placeholder, and then strips out
    # everything except for the gif name for later testing of fetching this
    # image.
    match = re.search('[^_]src="/.*/(.*1.*.gif)[^"]*', body)
    assert match

    blank_gif_src = match.group(1)
    print blank_gif_src

    # Fetch the blank image and make sure it's served correctly.
    proxy_url = "http://%s/%s/%s" %\
        (config.PROXY_DOMAIN, config.PSA_JS_LIBRARY_URL_PREFIX, blank_gif_src)

    resp, body = helpers.fetch(proxy_url)
    assert resp.getheader("cache-control") == "max-age=31536000"

# Checks that lazyload_images,debug injects non-optimized javascript from
# lazyload_images.js. The debug JS will still have comments stripped, since we
# run it through the closure compiler to resolve any uses of goog.require.
def test_lazyload_images_debug_mode():
    url = ("%s/lazyload_images.html?PageSpeedFilters=lazyload_images,debug" %
        config.EXAMPLE_ROOT)
    _resp, body = helpers.fetch(url)
    assert body.count("pagespeed.lazyLoad")
    assert not re.search('/\*', body)
    assert not body.count("goog.require")
    assert body.count("PageSpeed=noscript")
