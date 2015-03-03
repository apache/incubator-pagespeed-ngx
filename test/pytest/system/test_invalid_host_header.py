import config
import test_helpers as helpers

proxy = config.SECONDARY_SERVER

def test_invalid_host_url_does_not_crash_the_server():
    url  = "http://127.0.0.\230:8080/"
    result = helpers.fetch(url, proxy = proxy, allow_error_responses = True)
    assert result.resp.status == 400 # Bad request
