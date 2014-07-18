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
 * @fileoverview Code for 'show caches' and 'purge cache' UI on caches page.
 *
 * @author xqyin@google.com (XiaoQian Yin)
 */

goog.provide('pagespeed.Caches');

goog.require('goog.events');
goog.require('goog.object');
goog.require('goog.string');



/**
 * @constructor
 */
pagespeed.Caches = function() {
  // The navigation bar to switch among different display modes.
  var modeElement = document.createElement('table');
  modeElement.id = 'mode_bar';
  modeElement.innerHTML =
      '<tr><td><a id="' +
      pagespeed.Caches.DisplayMode.METADATA_CACHE +
      '" href="javascript:void(0);">' +
      'Show Metadata Cache</a> - </td>' +
      '<td><a id="' +
      pagespeed.Caches.DisplayMode.CACHE_STRUCTURE +
      '" href="javascript:void(0);">' +
      'Caches Structure</a> - </td>' +
      '<td><a id="' +
      pagespeed.Caches.DisplayMode.PURGE_CACHE +
      '" href="javascript:void(0);">' +
      'Purge Cache</a></td></tr>';
  document.body.insertBefore(
      modeElement,
      document.getElementById(pagespeed.Caches.DisplayDiv.METADATA_CACHE));
  // TODO(xqyin): Maybe use closure tab pane UI element instead.
};


/**
 * Display the detailed structure of selected cache.
 * @param {string} id The id of the div element to display.
 * @export
 */
pagespeed.Caches.toggleDetail = function(id) {
  var toggle_button = document.getElementById(id + '_toggle');
  var summary_div = document.getElementById(id + '_summary');
  var detail_div = document.getElementById(id + '_detail');
  if (toggle_button.checked) {
    summary_div.style.display = 'none';
    detail_div.style.display = 'block';
  } else {
    summary_div.style.display = 'block';
    detail_div.style.display = 'none';
  }
};


/**
 * Enum for display mode values. These are the ids of the links on the
 * navigation bar. Users click the link and call show(div) method to display
 * the corresponding div element.
 * @enum {string}
 */
pagespeed.Caches.DisplayMode = {
  METADATA_CACHE: 'metadata_mode',
  CACHE_STRUCTURE: 'struct_mode',
  PURGE_CACHE: 'purge_mode'
};


/**
 * Enum for display div values. These are the ids of the div elements that
 * contain actual content. Only the chosen div element will be displayed on
 * the page. Others will be hidden.
 * @enum {string}
 */
pagespeed.Caches.DisplayDiv = {
  METADATA_CACHE: 'show_metadata',
  CACHE_STRUCTURE: 'cache_struct',
  PURGE_CACHE: 'purge_cache'
};


/**
 * Parse the location URL to get its anchor part and show the div element.
 */
pagespeed.Caches.prototype.parseLocation = function() {
  var div = location.hash.substr(1);
  if (div == '') {
    this.show(pagespeed.Caches.DisplayDiv.METADATA_CACHE);
  } else if (goog.object.contains(pagespeed.Caches.DisplayDiv, div)) {
    this.show(div);
  }
};


/**
 * Show the corresponding div element of chosen display mode.
 * @param {string} div The div element to display according to the current
 *     display mode.
 */
pagespeed.Caches.prototype.show = function(div) {
  // Hides all the elements first.
  document.getElementById(
      pagespeed.Caches.DisplayDiv.METADATA_CACHE).style.display = 'none';
  document.getElementById(
      pagespeed.Caches.DisplayDiv.CACHE_STRUCTURE).style.display = 'none';
  document.getElementById(
      pagespeed.Caches.DisplayDiv.PURGE_CACHE).style.display = 'none';
  // Only shows the element chosen by users.
  document.getElementById(div).style.display = '';
  location.href = location.href.split('#')[0] + '#' + div;
};


/**
 * Present a form to enter a URL for cache purging.
 */
pagespeed.Caches.prototype.purgeInit = function() {
  var purgeUI = document.createElement('form');
  purgeUI.method = 'get';
  purgeUI.innerHTML =
      'URL: <input type="text" name="purge" size="110" /><br>' +
      '<input type="submit" value="Purge Individual URL">';
  var purgeElement = document.getElementById(
      pagespeed.Caches.DisplayDiv.PURGE_CACHE);
  purgeElement.insertBefore(purgeUI, purgeElement.firstChild);
};


/**
 * The Main entry to start processing.
 * @export
 */
pagespeed.Caches.Start = function() {
  var cachesOnload = function() {
    var cachesObj = new pagespeed.Caches();
    cachesObj.purgeInit();
    cachesObj.parseLocation();
    goog.events.listen(
        document.getElementById(pagespeed.Caches.DisplayMode.METADATA_CACHE),
        'click',
        goog.bind(cachesObj.show, cachesObj,
                  pagespeed.Caches.DisplayDiv.METADATA_CACHE));

    goog.events.listen(
        document.getElementById(pagespeed.Caches.DisplayMode.CACHE_STRUCTURE),
        'click',
        goog.bind(cachesObj.show, cachesObj,
                  pagespeed.Caches.DisplayDiv.CACHE_STRUCTURE));

    goog.events.listen(
        document.getElementById(pagespeed.Caches.DisplayMode.PURGE_CACHE),
        'click',
        goog.bind(cachesObj.show, cachesObj,
                  pagespeed.Caches.DisplayDiv.PURGE_CACHE));
    // IE6/7 don't support this event. Then the back button would not work
    // because no requests captured.
    goog.events.listen(window, 'hashchange',
                       goog.bind(cachesObj.parseLocation, cachesObj));
  };
  goog.events.listen(window, 'load', cachesOnload);
};
