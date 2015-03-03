import config
import test_helpers as helpers

# Fetch with the default limit so our test file is inlined.
def test_flatten_css_imports_rewrite_css_default_limit():
    filter_name = "flatten_css_imports,rewrite_css"
    url = "%s/flatten_css_imports.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)
    headers = {"X-PSA-Blocking-Rewrite" : "psatest"}
    _resp, body = helpers.fetch(url, headers)
    assert body.count("@import url") == 0
    assert body.count("yellow{background-color:")

# Fetch with a tiny limit so no file can be inlined.
def test_flatten_css_imports_rewrite_css_tiny_limit():
    filter_name = "flatten_css_imports,rewrite_css"
    url = "%s/flatten_css_imports.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)
    headers = {"X-PSA-Blocking-Rewrite" : "psatest",
        "PageSpeedCssFlattenMaxBytes" : "5"}

    _resp, body = helpers.fetch(url, headers)
    assert body.count("@import url")
    assert body.count("yellow{background-color:") == 0

# Fetch with a medium limit so any one file can be inlined but not all of them.
def test_flatten_css_imports_rewrite_css_medium_limit():
    filter_name = "flatten_css_imports,rewrite_css"
    url = "%s/flatten_css_imports.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name)
    headers = {"X-PSA-Blocking-Rewrite" : "psatest",
        "PageSpeedCssFlattenMaxBytes" : "50"}

    _resp, body = helpers.fetch(url, headers)
    assert body.count("@import url")
    assert body.count("yellow{background-color:") == 0



