/*
 * Copyright 2014 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview casperjs tests for admin site page.
 * @author jcrowell@google.com (Jeffrey Crowell)
 */

// Allow for xpath selections.
var x = require('casper').selectXPath;

casper.test.begin('Statistics page can load and filter works.',
                  12, function(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/statistics', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() {
    // Check that we have all of the stats available.
    var stats = this.evaluate(function() {
      var stats_frac = document.querySelector('#num').innerHTML;
      return stats_frac.split(' ').pop().split('/');
    });
    var numerator = parseInt(stats[0]);
    var denominator = parseInt(stats[1]);
    test.assertEquals(numerator, denominator);
    test.assertEquals(numerator, denominator);
    test.assert(numerator > 0);
    var stats_arr = this.evaluate(function() {
      return document.querySelector('#stat').innerHTML.split('<td');
    });
    test.assert(stats_arr.length > 10);
  });

  casper.then(function() {
    // Fill out the Filter with something that 2 stats should match.
    this.sendKeys('#text-filter', 'cache_backend');
    var stats = this.evaluate(function() {
      var stats_frac = document.querySelector('#num').innerHTML;
      return stats_frac.split(' ').pop().split('/');
    });
    var numerator = parseInt(stats[0]);
    var denominator = parseInt(stats[1]);
    test.assertEquals(numerator, 2);
    test.assert(denominator > 0);
    var stats_arr = this.evaluate(function() {
      return document.querySelector('#stat').innerHTML.split('<td');
    });
    test.assert(stats_arr.length > 5);
  });

  casper.then(function() {
    // Fill out the Filter with something that no stats should match.
    this.sendKeys('#text-filter',
                  'itislikelythatnothingwillmatchthisverylongstring');
    var stats = this.evaluate(function() {
      var stats_frac = document.querySelector('#num').innerHTML;
      return stats_frac.split(' ').pop().split('/');
    });
    var numerator = parseInt(stats[0]);
    var denominator = parseInt(stats[1]);
    test.assertEquals(numerator, 0);
    test.assert(denominator > 0);
    stats_arr = this.evaluate(function() {
      return document.querySelector('#stat').innerHTML.split('<td');
    });
    test.assertEquals(stats_arr.length, 4);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Configuration Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/config', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Histograms Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/histograms', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Metadata Caches Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start(
      'http://localhost/pagespeed_admin/cache#show_metadata?PageSpeedFilters=+debug',
      function() {
        // Check that the page loaded properly with no errors.
        test.assertEquals(errors, []);
      });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Cache Structure Page', 6, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start(
      'http://localhost/pagespeed_admin/cache#show_metadata', function() {
        // Check that the page loaded properly with no errors.
        test.assertEquals(errors, []);
      });

  casper.thenOpen('http://localhost/pagespeed_admin/cache#cache_struct');

  casper.then(function() {
    // Click all of the checkboxes.
    this.test.assertExists(
        {type: 'xpath', path: '//*[@id="HTTP Cache_toggle"]'});
    this.click(x('//*[@id="HTTP Cache_toggle"]'));
    this.test.assertExists(
        {type: 'xpath', path: '//*[@id="Metadata Cache_toggle"]'});
    this.click(x('//*[@id="Metadata Cache_toggle"]'));
    this.test.assertExists(
        {type: 'xpath', path: '//*[@id="Property Cache_toggle"]'});
    this.click(x('//*[@id="Property Cache_toggle"]'));
    this.test.assertExists(
        {type: 'xpath', path: '//*[@id="FileSystem Metadata Cache_toggle"]'});
    this.click(x('//*[@id="FileSystem Metadata Cache_toggle"]'));
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Console Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/console', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Message History Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/message_history', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});

casper.test.begin('Check Graphs Page', 2, function suite(test) {
  errors = [];
  casper.on('page.error', function(msg, trace) { errors.push(msg); });

  casper.start('http://localhost/pagespeed_admin/graphs', function() {
    // Check that the page loaded properly with no errors.
    test.assertEquals(errors, []);
  });

  casper.then(function() { test.assertEquals(errors, []); });

  casper.run(function() { test.done(); });
});
