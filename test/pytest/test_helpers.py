from collections import namedtuple
from copy import copy
import itertools
import re
from socket import error as socket_error
import time
import urllib3
from urlparse import urljoin
from wsgiref.handlers import format_date_time

import config
from config import log

FetchResult = namedtuple('FetchResult', 'resp body')
fetch_count = itertools.count()

# We keep this pool managers around globally, as that seems to enable
# connection re-use between fetches for the tests.
# Doing so for proxied requests too needs investigation, as that seems to
# cause fetches to timeout while reading sometimes.
http = urllib3.PoolManager()

def patternCountEquals(result, pattern, count):
    return len(re.findall(pattern, result.body)) == count

def stringCountEquals(result, substring, count):
    return result.body.count(substring) == count

# Wrapper around the fetch generator, providing convenience methods like
# waitFor()
class FetchUntil:
    def __init__(self, url, *args, **kwargs):
        self.it = fetch_generator(url, *args, **kwargs)

    def __iter__(self):
        return self

    def next(self):
        return self.it.next()

    # Perform http requests until the predicate evaluates the response to True.
    # Current time limit is 5 seconds, 0.1 second sleep will be done between
    # subsequent tries.
    # Any named arguments will be passed on to fetch_url.
    def waitFor(self, predicate, *predicate_args):
        res = self.next()
        while True:
            if predicate(res, *predicate_args):
                return res, True
            try:
                res = self.next()
            except StopIteration:
                break

        return res, False

# Fetch the given url. config.DEFAULT_USER_AGENT will be used if no User-Agent
# request header is specified.
# Will assert on receiving error responses, unless allow_error_responses is set.
# Relative urls will be absolutified to the first match of:
# 1. The host header
# 2. The given proxy
# 3. The primary test host
def fetch(
    url, headers = None, timeout = None, proxy = "",
    method = "GET", allow_error_responses = False):
    mycount = fetch_count.next()
    headers = {} if headers is None else copy(headers)
    timeout = urllib3.Timeout(total=10.0) if timeout is None else timeout
    if not "User-Agent" in headers:
        headers["User-Agent"] = config.DEFAULT_USER_AGENT

    if not url.startswith("http"):
        if not "Host" in headers:
            if proxy:
                url = "%s%s" % (proxy, url)
            else:
                url = "%s%s" % (config.PRIMARY_SERVER, url)


    # Helps cross referencing between this and server log
    headers["PSOL-Fetch-Id"] = mycount
    fetch_method = None
    log.debug("[%d:] fetch_url %s %s: %s" % (mycount, method, url, headers))
    try:
        if proxy:
            fetch_method = urllib3.ProxyManager(proxy).request
        else:
            fetch_method = http.request

        resp = fetch_method(method, url, headers = headers, timeout = timeout,
            retries = False, redirect = False)
    except:
        log.debug("[%d]: fetch_url excepted", mycount)
        raise
    body = resp.data

    log.debug("[%d]: fetch_url %s %s\n%s" %
        (mycount, resp.status, resp.getheaders(), body))

    if not allow_error_responses:
        assert resp.status < 400, resp.status

    return FetchResult(resp, body)

# Yields fetches for a fixed period, passing on args/kwargs to fetch.
def fetch_generator(url, *args, **kwargs):
    timeout_seconds = time.time() + 5
    ok = True
    while ok:
        res = fetch(url, *args, **kwargs)
        yield res
        ok = time.time() < timeout_seconds
        if ok:
            time.sleep(0.05)
    log.debug("Fetch generator timed out")


# Transform the passed in date into a formatted string usable in
# http Date: headers
def http_date(d):
    stamp = time.mktime(d.timetuple())
    return format_date_time(stamp)

def absolutify_url(base_url, relative_url):
    return urljoin(base_url, relative_url)

def internal_get_stat(url, stat):
    stat = "%s:" % stat
    result = fetch(url)
    line = [l for l in result.body.split("\n") if l.startswith(stat)][0]
    return int(line[len(stat):].replace(" ", ""))

def assert_stat_equals(stat, val):
    assert internal_get_stat(config.STATISTICS_URL, stat) == val

def assert_wait_for_stat_to_equal(stat, expected_value):
    timeout_seconds = time.time() + 5
    last_stat_value = None
    while time.time() < timeout_seconds:
        last_stat_value = internal_get_stat(config.STATISTICS_URL, stat)
        if last_stat_value == expected_value:
            return
        time.sleep(0.2)

    log.debug("wait for stat [%s] timed out, last value: %s, expected: %s"
        % (stat, last_stat_value, expected_value))

    assert last_stat_value == expected_value

