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
 * @fileoverview Implements auto-refresh and filtering for statistics page.
 *
 * @author xqyin@google.com (XiaoQian Yin)
 */

goog.provide('pagespeed.Statistics');

goog.require('goog.array');
goog.require('goog.events');
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.require('goog.string');



/**
 * @constructor
 * @param {goog.testing.net.XhrIo=} opt_xhr Optional mock XmlHttpRequests
 *     handler for testing.
 */
pagespeed.Statistics = function(opt_xhr) {
  /**
   * The XmlHttpRequests handler for auto-refresh. We pass a mock XhrIo
   * when testing. Default is a normal XhrIo.
   * @private {goog.net.XhrIo|goog.testing.net.XhrIo}
   */
  this.xhr_ = opt_xhr || new goog.net.XhrIo();

  /**
   * The most recent statistics list.
   * @private {!Array.<string>}
   */
  this.psolMessages_ = document.getElementById('stat').textContent.split('\n');
  // The element with id 'stat' must exist.
  if (this.psolMessages_.length > 0) {
    // Remove the empty entry.
    this.psolMessages_.pop();
  }

  /**
   * We provide filtering functionality for users to search for certain
   * statistics like all the statistics related to caches. This option is the
   * matching pattern. Filtering is case-insensitive.
   * @private {string}
   */
  this.filter_ = '';

  /**
   * The option of auto-refresh. If true, the page will automatically refresh
   * itself. The default frequency is to refresh every 5 seconds.
   * @private {boolean}
   */
  this.autoRefresh_ = false;

  var wrapper = document.createElement('div');
  wrapper.style.overflow = 'hidden';
  wrapper.style.clear = 'both';

  // The UI table of auto-refresh and filtering.
  var uiTable = document.createElement('div');
  uiTable.id = 'ui-div';
  uiTable.innerHTML =
      '<table id="ui-table" border=1 style="float:left; border-collapse: ' +
      'collapse;border-color:silver;"><tr valign="center">' +
      '<td>Auto refresh (every 5 seconds): <input type="checkbox" ' +
      'id="auto-refresh" ' + (this.autoRefresh_ ? 'checked' : '') +
      '></td><td>&nbsp;&nbsp;&nbsp;&nbsp;Filter: ' +
      '<input id="text-filter" type="text" size="70"></td>' +
      '</tr></table>';
  wrapper.appendChild(uiTable);

  var numElement = document.createElement('div');
  numElement.id = 'num';
  numElement.className = 'pagespeed-show-number';
  wrapper.appendChild(numElement);

  document.body.insertBefore(wrapper, document.getElementById('stat'));

  this.updateMessageCount();
};


/**
 * Updates the option of auto-refresh.
 */
pagespeed.Statistics.prototype.toggleAutorefresh = function() {
  this.autoRefresh_ = !this.autoRefresh_;
};


/**
 * Updates the option of statistics filter.
 * @param {!HTMLElement} element The HTML element of Filter.
 */
pagespeed.Statistics.prototype.setFilter = function(element) {
  this.filter_ = element.value;
  this.update();
};


/**
 * Update the text showing the number of statistics.
 * @param {number=} opt_num An optional specified number to show when
 *     filtering the messages.
 */
pagespeed.Statistics.prototype.updateMessageCount = function(opt_num) {
  // The default number of messages is the length of the array when opt_num
  // is not specified.
  var total = (opt_num != undefined) ? opt_num : this.psolMessages_.length;
  document.getElementById('num').textContent =
      'The number of statistics: ' + total.toString();
};


/**
 * Show the error message.
 */
pagespeed.Statistics.prototype.error = function() {
  goog.array.clear(this.psolMessages_);
  this.updateMessageCount();
  document.getElementById('stat').textContent =
      pagespeed.Statistics.REFRESH_ERROR_;
};


/**
 * Filters and displays updated statistics.
 */
pagespeed.Statistics.prototype.update = function() {
  var messages = goog.array.clone(this.psolMessages_);
  if (this.filter_) {
    for (var i = messages.length - 1; i >= 0; --i) {
      if (!messages[i] ||
          !goog.string.caseInsensitiveContains(messages[i], this.filter_)) {
        messages.splice(i, 1);
      }
    }
  }
  this.updateMessageCount(messages.length);
  var statElement = document.getElementById('stat');
  statElement.textContent = messages.join('\n');
};


/**
 * Parses new statistics from the server response.
 * @param {!Object} jsonData The data dumped in JSON format.
 */
pagespeed.Statistics.prototype.parseMessagesFromResponse = function(jsonData) {
  var variables = jsonData['variables'];
  var maxLength = jsonData['maxlength'];

  // Check if the response is valid JSON.
  if (goog.typeOf(variables) != 'object' ||
      goog.typeOf(maxLength) != 'number') {
    this.error();
    return;
  }

  var messages = [];
  for (var name in variables) {
    var numSpaces = maxLength - name.length -
                    variables[name].toString().length;
    var line = name + ':' + new Array(numSpaces + 2).join(' ') +
               variables[name].toString();
    messages.push(line);
  }
  this.psolMessages_ = messages;
  this.update();
};


/**
  * The error message of auto-refresh.
  * @private {string}
  * @const
  */
pagespeed.Statistics.REFRESH_ERROR_ =
    'Sorry, failed to update the statistics. ' +
    'Please wait and try again later.';


/**
 * Capture the pathname and return the URL for request.
 * @return {string} The URL to request the updated data in JSON format.
 */
pagespeed.Statistics.prototype.requestUrl = function() {
  var pathName = location.pathname;
  var n = pathName.lastIndexOf('/');
  // Ignore the ending '/'.
  if (n == pathName.length - 1) {
    n = pathName.substring(0, n).lastIndexOf('/');
  }
  if (n > 0) {
    return pathName.substring(0, n) + '/stats_json';
  } else {
    return '/stats_json';
  }
};


/**
 * Refreshes the page by making requsts to server.
 */
pagespeed.Statistics.prototype.performRefresh = function() {
  if (!this.xhr_.isActive() && this.autoRefresh_) {
    this.xhr_.send(this.requestUrl());
  }
};


/**
 * Parses the response sent by server and updates the page.
 */
pagespeed.Statistics.prototype.parseAjaxResponse = function() {
  if (this.xhr_.isSuccess()) {
    var jsonData = this.xhr_.getResponseJson();
    this.parseMessagesFromResponse(jsonData);
  } else {
    console.log(this.xhr_.getLastError());
    this.error();
  }
};


/**
 * The frequency of auto-refresh. Default is once per 5 seconds.
 * @private {number}
 * @const
 */
pagespeed.Statistics.FREQUENCY_ = 5 * 1000;


/**
 * The Main entry to start processing.
 * @export
 */
pagespeed.Statistics.Start = function() {
  var statisticsOnload = function() {
    var statisticsObj = new pagespeed.Statistics();
    var filterElement = document.getElementById('text-filter');
    goog.events.listen(
        filterElement, 'keyup', goog.bind(statisticsObj.setFilter,
                                          statisticsObj, filterElement));
    goog.events.listen(document.getElementById('auto-refresh'), 'change',
                       goog.bind(statisticsObj.toggleAutorefresh,
                                 statisticsObj));
    setInterval(statisticsObj.performRefresh.bind(statisticsObj),
                pagespeed.Statistics.FREQUENCY_);
    goog.events.listen(
        statisticsObj.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(statisticsObj.parseAjaxResponse, statisticsObj));
    statisticsObj.performRefresh();
  };
  goog.events.listen(window, 'load', statisticsOnload);
};
