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
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.require('goog.object');
goog.require('goog.string');



/**
 * @constructor
 * @param {goog.testing.net.XhrIo=} opt_xhr Optional mock XmlHttpRequests
 *     handler for testing.
 */
pagespeed.Caches = function(opt_xhr) {
  /**
   * The XmlHttpRequests handler used for cache purging and showing metadata
   * cache entry. We pass a mock XhrIo when testing. Default is a normal XhrIo.
   * @private {goog.net.XhrIo|goog.testing.net.XhrIo}
   */
  this.xhr_ = opt_xhr || new goog.net.XhrIo();

  /**
   * A string to indicate whether the current input is from metadata tab or
   * purge cache tab.
   * @private {string}
   */
  this.inputIsFrom_ = '';

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
      'Show Cache Structure</a> - </td>' +
      '<td><a id="' +
      pagespeed.Caches.DisplayMode.PHYSICAL_CACHE +
      '" href="javascript:void(0);">' +
      'Physical Caches</a> - </td>' +
      '<td><a id="' +
      pagespeed.Caches.DisplayMode.PURGE_CACHE +
      '" href="javascript:void(0);">' +
      'Purge Cache</a></td></tr>';
  document.body.insertBefore(
      modeElement,
      document.getElementById(pagespeed.Caches.DisplayDiv.METADATA_CACHE));
  // TODO(xqyin): Maybe use closure tab pane UI element instead.

  // Create a div to display the result of showing metadata cache entry.
  var metadataResult = document.createElement('pre');
  metadataResult.id = pagespeed.Caches.ElementId.METADATA_RESULT;
  // TODO(xqyin): Move stylistic elements to a CSS file.
  metadataResult.style.padding = '10px 0px 0px 5px';
  metadataResult.style.fontFamily = 'verdana';
  var metadataElement = document.getElementById(
      pagespeed.Caches.DisplayDiv.METADATA_CACHE);
  metadataElement.appendChild(metadataResult);

  // Create a div to display the result of cache purging.
  var purgeResult = document.createElement('pre');
  purgeResult.id = pagespeed.Caches.ElementId.PURGE_RESULT;
  purgeResult.style.padding = '10px 0px 0px 5px';
  purgeResult.style.fontFamily = 'verdana';
  var purgeElement = document.getElementById(
      pagespeed.Caches.DisplayDiv.PURGE_CACHE);
  purgeElement.insertBefore(purgeResult, purgeElement.firstChild);
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
  METADATA_CACHE: 'show_metadata_mode',
  CACHE_STRUCTURE: 'cache_struct_mode',
  PHYSICAL_CACHE: 'physical_cache_mode',
  PURGE_CACHE: 'purge_cache_mode'
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
  PHYSICAL_CACHE: 'physical_cache',
  PURGE_CACHE: 'purge_cache'
};


/**
 * Enum for ids of elements that will be used in cache purging and showing
 * metadata cache.
 */
pagespeed.Caches.ElementId = {
  METADATA_TEXT: 'metadata_text',
  METADATA_SUBMIT: 'metadata_submit',
  METADATA_RESULT: 'metadata_result',
  METADATA_CLEAR: 'metadata_clear',
  PURGE_TEXT: 'purge_text',
  PURGE_SUBMIT: 'purge_submit',
  PURGE_RESULT: 'purge_result',
  PURGE_ALL: 'purge_all',
  PURGE_SET: 'purge_set',
  USER_AGENT: 'user_agent'
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
  // Only shows the div chosen by users.
  for (var i in pagespeed.Caches.DisplayDiv) {
    var chartDiv = pagespeed.Caches.DisplayDiv[i];
    if (chartDiv == div) {
      document.getElementById(chartDiv).style.display = '';
    } else {
      document.getElementById(chartDiv).style.display = 'none';
    }
  }

  // Underline the current tab.
  var currentTab = document.getElementById(div + '_mode');
  for (var i in pagespeed.Caches.DisplayMode) {
    var link = document.getElementById(pagespeed.Caches.DisplayMode[i]);
    if (link == currentTab) {
      link.style.textDecoration = 'underline';
      link.style.color = 'darkblue';
    } else {
      link.style.textDecoration = '';
      link.style.color = '';
    }
  }

  location.href = location.href.split('#')[0] + '#' + div;
};


/**
 * Send the cache purging URL to the sever.
 */
pagespeed.Caches.prototype.sendPurgeRequest = function() {
  if (!this.xhr_.isActive()) {
    var urlId = pagespeed.Caches.ElementId.PURGE_TEXT;
    var url =
        '?purge=' +
        encodeURIComponent(document.getElementById(urlId).value);
    this.inputIsFrom_ = pagespeed.Caches.ElementId.PURGE_RESULT;
    this.xhr_.send(url);
  }
};


/**
 * Send '*' to server to purge the entire cache.
 */
pagespeed.Caches.prototype.sendPurgeAllRequest = function() {
  if (!this.xhr_.isActive()) {
    var url = '?purge=*';
    this.inputIsFrom_ = pagespeed.Caches.ElementId.PURGE_RESULT;
    this.xhr_.send(url);
  }
};


/**
 * Send request to get the updated purge set.
 */
pagespeed.Caches.prototype.sendPurgeSetRequest = function() {
  if (!this.xhr_.isActive()) {
    var url = '?new_set=';
    this.inputIsFrom_ = pagespeed.Caches.ElementId.PURGE_SET;
    this.xhr_.send(url);
  }
};


/**
 * Send the cache purging URL to the sever.
 */
pagespeed.Caches.prototype.sendMetadataRequest = function() {
  if (!this.xhr_.isActive()) {
    var urlId = pagespeed.Caches.ElementId.METADATA_TEXT;
    var userAgentId = pagespeed.Caches.ElementId.USER_AGENT;
    var url =
        '?url=' +
        encodeURIComponent(document.getElementById(urlId).value) +
        '&user_agent=' +
        encodeURIComponent(document.getElementById(userAgentId).value);
    this.inputIsFrom_ = pagespeed.Caches.ElementId.METADATA_RESULT;
    this.xhr_.send(url);
  }
};


/**
 * Display the result of cache purging and showing metadata cache entry.
 */
pagespeed.Caches.prototype.showResult = function() {
  if (this.xhr_.isSuccess()) {
    var text = this.xhr_.getResponseText();
    if (this.inputIsFrom_ == pagespeed.Caches.ElementId.PURGE_RESULT) {
      // Add updating purge set to the queue if the current request is
      // to purge cache.
      window.setTimeout(goog.bind(this.sendPurgeSetRequest, this), 0);
    }
    document.getElementById(this.inputIsFrom_).textContent = text;
  } else {
    console.log(this.xhr_.getLastError());
  }
};


/**
 * Present a form to enter a URL for cache purging.
 */
pagespeed.Caches.prototype.purgeInit = function() {
  // It's actually not a 'form' element because we add listeners to the
  // buttons to submit query params. We don't want users to submit the
  // form by pressing enter without clicking the buttons.
  var purgeUI = document.createElement('table');
  purgeUI.innerHTML =
      'URL: <input id="' + pagespeed.Caches.ElementId.PURGE_TEXT +
      '" type="text" name="purge" size="110"/><br>' +
      '<input id="' + pagespeed.Caches.ElementId.PURGE_SUBMIT +
      '" type="button" value="Purge Individual URL"/>' +
      '<input id="' + pagespeed.Caches.ElementId.PURGE_ALL +
      '" type="button" value="Purge Entire Cache"/>';
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
    for (var i in pagespeed.Caches.DisplayDiv) {
      goog.events.listen(
          document.getElementById(pagespeed.Caches.DisplayMode[i]), 'click',
          goog.bind(cachesObj.show, cachesObj, pagespeed.Caches.DisplayDiv[i]));
    }
    // IE6/7 don't support this event. Then the back button would not work
    // because no requests captured.
    goog.events.listen(window, 'hashchange',
                       goog.bind(cachesObj.parseLocation, cachesObj));
    goog.events.listen(
        document.getElementById(pagespeed.Caches.ElementId.PURGE_SUBMIT),
        'click', goog.bind(cachesObj.sendPurgeRequest, cachesObj));
    goog.events.listen(
        document.getElementById(pagespeed.Caches.ElementId.PURGE_ALL),
        'click', goog.bind(cachesObj.sendPurgeAllRequest, cachesObj));
    goog.events.listen(
        document.getElementById(pagespeed.Caches.ElementId.METADATA_SUBMIT),
        'click', goog.bind(cachesObj.sendMetadataRequest, cachesObj));
    goog.events.listen(
        cachesObj.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(cachesObj.showResult, cachesObj));
    goog.events.listen(
        document.getElementById(pagespeed.Caches.ElementId.METADATA_CLEAR),
        'click', location.reload.bind(location));
  };
  goog.events.listen(window, 'load', cachesOnload);
};
