import config
import test_helpers as helpers

# Test DNS prefetching. DNS prefetching is dependent on user agent, but is
# enabled for Wget UAs, allowing this test to work with our default wget params.
def test_insert_dns_prefetch():
    filter_name = "insert_dns_prefetch"
    url = "%s/%s.html?PageSpeedFilters=%s" % (
        config.EXAMPLE_ROOT, filter_name, filter_name)

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "//ref.pssdemos.com", 2)
    assert success, result.body

    result, success = helpers.FetchUntil(url).waitFor(
        helpers.stringCountEquals, "//ajax.googleapis.com", 2)
    assert success, result.body
