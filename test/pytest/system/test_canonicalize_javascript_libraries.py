import config
import test_helpers as helpers


def test_canonicalize_javascript_libraries_finds_library_urls():
    # Checks that we can correctly identify a known library url.
    url = ("%s/canonicalize_javascript_libraries.html?PageSpeedFilters="
          "canonicalize_javascript_libraries" % config.EXAMPLE_ROOT)
    expect  = "http://www.modpagespeed.com/rewrite_javascript.js"

    last, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, expect, 1)
    assert success, last.body
