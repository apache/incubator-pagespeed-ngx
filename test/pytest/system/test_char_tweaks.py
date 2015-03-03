import re

import config
import test_helpers as helpers


def test_collapse_whitespace_removes_whitespace_but_not_from_pre_tags():
    url = ("%s/collapse_whitespace.html?PageSpeedFilters=collapse_whitespace" %
        config.EXAMPLE_ROOT)
    result = helpers.fetch(url)
    assert len(re.findall(r'^ +<', result.body, re.MULTILINE)) == 1


def test_pedantic_adds_default_type_attributes():
    url = "%s/pedantic.html?PageSpeedFilters=pedantic" % config.EXAMPLE_ROOT
    result = helpers.fetch(url)
    assert result.body.count("text/javascript") > 0  # should find script type
    assert result.body.count("text/css") > 0        # should find style type


def test_remove_comments_removes_comments_but_not_IE_directives():
    url = ("%s/remove_comments.html?PageSpeedFilters=remove_comments" %
        config.EXAMPLE_ROOT)
    result = helpers.fetch(url)
    assert result.body.count("removed") == 0  # comment, should not find
    assert result.body.count("preserved") > 0  # preserves IE directives


def test_remove_quotes_does_what_it_says_on_the_tin():
    url = ("%s/remove_quotes.html?PageSpeedFilters=remove_quotes" %
        config.EXAMPLE_ROOT)
    result = helpers.fetch(url)
    # 4 quoted attrs
    assert len(re.findall(r'"', result.body, re.MULTILINE)) == 4
    assert result.body.count("'") == 0  # no apostrophes


def test_trim_urls_makes_urls_relative():
    url = "%s/trim_urls.html?PageSpeedFilters=trim_urls" % config.EXAMPLE_ROOT
    result = helpers.fetch(url)
    # base dir, shouldn't find
    assert result.body.count("mod_pagespeed_example") == 0
    assert len(result.body) < 153  # down from 157
