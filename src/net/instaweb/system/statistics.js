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


/** @typedef {{name: string, value:string}} */
pagespeed.StatsNode;



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
   * @private {Array.<pagespeed.StatsNode>}
   */
  this.psolMessages_ = [];
  var messages = document.getElementById('stat').textContent.split('\n');
  for (var i = 0; i < messages.length; ++i) {
    var tmp = messages[i].split(':');
    if (tmp.length < 2) continue;
    var node = {
      name: tmp[0].trim(),
      value: tmp[1].trim()
    };
    this.psolMessages_.push(node);
  }

  /**
   * The total number of unfiltered messages.
   * @private {number}
   */
  this.numUnfilteredMessages_ = this.psolMessages_.length;

  /**
   * The statistics list after last refresh. The type of this object is like
   * the initializer below. this.lastValue_['cache_hits'] is the value of
   * 'cache_hits' after last refresh.
   * @private {!Object}
   */
  this.lastValue_ = {'statsName': 0};

  /**
   * Heuristic for how often each variable changes. It basically
   * counts the number of changes we've noticed since the first
   * refresh. The exact value is not important, instead this is used
   * to distinguish "more active" stats from "less active" ones.
   *
   * For example, this.timesOfChange_['cache_hits'] is the number of
   * times of changes of 'cache_hits' since the first refresh.
   * @private {!Object}
   */
  this.timesOfChange_ = {'statsName': 0};

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

  /**
   * If this option is false, the statistics is sorted alphabetically. When it
   * is set to true, the statistics is sorted in descending order of the
   * number of changes and secondarily alphabetic order.
   * @private {boolean}
   */
  this.sortByNumberOfChanges_ = false;

  /**
   * The flag of whether the first refresh is started. We need to call a
   * refresh when loading the page to sort statistics. Default is false,
   * which means the page hasn't started the first refresh yet.
   * @private {boolean}
   */
  this.firstRefreshStarted_ = false;

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
      'id="auto-refresh" ' + (this.autoRefresh_ ? 'checked' : '') + '></td>' +
      '<td>&nbsp;&nbsp;&nbsp;&nbsp;Filter: ' +
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
 * Updates the option of sort-by-number-of-changes.
 */
pagespeed.Statistics.prototype.toggleSorting = function() {
  this.sortByNumberOfChanges_ = !this.sortByNumberOfChanges_;
  this.update();
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
      'The number of statistics: ' + total.toString() + '/' +
      this.numUnfilteredMessages_;
};


/**
 * Show the error message.
 */
pagespeed.Statistics.prototype.error = function() {
  goog.array.clear(this.psolMessages_);
  this.numUnfilteredMessages_ = 0;
  this.updateMessageCount();
  document.getElementById('stat').textContent =
      pagespeed.Statistics.REFRESH_ERROR_;
};


/**
 * Filters and displays updated statistics.
 */
pagespeed.Statistics.prototype.update = function() {
  if (this.sortByNumberOfChanges_) {
    // Sort in descending order of the number of changes.
    this.sortByNumberOfChanges();
  } else {
    // Sort in alphabetical order.
    this.sortByAlphabet();
  }

  var messages = goog.array.clone(this.psolMessages_);
  if (this.filter_) {
    for (var i = messages.length - 1; i >= 0; --i) {
      if (!messages[i].name ||
          !goog.string.caseInsensitiveContains(messages[i].name,
                                               this.filter_)) {
        messages.splice(i, 1);
      }
    }
  }
  this.updateMessageCount(messages.length);

  // From observation, it's much slower to add the table header in the
  // constructor and update the table rows when refreshing the page.
  // So we recreate the table here every time we call this.update().
  var table = document.createElement('table');
  for (var i = 0; i < messages.length; ++i) {
    var row = table.insertRow(-1);
    var cell = row.insertCell(0);
    cell.textContent = messages[i].name;
    cell.className = 'pagespeed-stats-name';
    cell = row.insertCell(1);
    cell.textContent = messages[i].value;
    cell.className = 'pagespeed-stats-value';
    cell = row.insertCell(2);
    cell.textContent =
        this.timesOfChange_[messages[i].name].toString();
    cell.className = 'pagespeed-stats-number-of-changes';
  }
  var header = table.createTHead().insertRow(0);
  var cell = header.insertCell(0);
  cell.innerHTML =
      'Name <input type="checkbox" id="sort-alpha" ' +
      'title="Sort in alphabetical order."' +
      (!this.sortByNumberOfChanges_ ? 'checked' : '') + '>';
  cell.className = 'pagespeed-stats-first-column';
  header.insertCell(1).textContent = 'Value';
  cell = header.insertCell(2);
  cell.innerHTML =
      'Number of changes <input type="checkbox" id="sort-freq" ' +
      'title="Sort by the number of changes(descending order)."' +
      (this.sortByNumberOfChanges_ ? 'checked' : '') + '>';
  cell.title =
      'How many times the value changes during the auto-refresh.\n' +
      'The number of changes only accumulates when auto-refresh is on.';
  cell.className = 'pagespeed-stats-third-column';
  var statElement = document.getElementById('stat');
  statElement.innerHTML = '';
  statElement.appendChild(table);
  goog.events.listen(document.getElementById('sort-freq'), 'change',
                     goog.bind(this.toggleSorting, this));
  goog.events.listen(document.getElementById('sort-alpha'), 'change',
                     goog.bind(this.toggleSorting, this));
};


/**
 * The comparison function to be passed into JS Array sort() method to
 * sort the statistics in alphabetic and ascending order.
 * @param {pagespeed.StatsNode} a The node to compare.
 * @param {pagespeed.StatsNode} b The node to compare.
 * @return {number} The return value of the comparison.
 */
pagespeed.Statistics.alphabetCompareFunc = function(a, b) {
  if (a.name > b.name) {
    return 1;
  } else if (a.name < b.name) {
    return -1;
  } else {
    return 0;
  }
};


/**
 * Sort the statistics by the number of changes.
 */
pagespeed.Statistics.prototype.sortByNumberOfChanges = function() {
  var compareFunc = function(a, b) {
    var first = this.timesOfChange_[a.name];
    var second = this.timesOfChange_[b.name];
    // Sort in the descending order of number of changes.
    if (second != first) {
      return second - first;
    } else {
      // Otherwise, sort in alphabetical order.
      return pagespeed.Statistics.alphabetCompareFunc(a, b);
    }
  };
  this.psolMessages_.sort(goog.bind(compareFunc, this));
};


/**
 * Sort the statistics in alphabetical order.
 */
pagespeed.Statistics.prototype.sortByAlphabet = function() {
  this.psolMessages_.sort(pagespeed.Statistics.alphabetCompareFunc);
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
    // Remove elements with trailing underscores.
    if (name[name.length - 1] == '_') continue;
    // For visual consistency, use underscores instead of a mix of dashes and
    // underscores.
    var trimmedName = name.replace(/-/g, '_');
    // Ignore the leading underscore.
    if (trimmedName[0] == '_') {
      trimmedName = trimmedName.substring(1);
    }
    var node = {
      name: trimmedName,
      value: variables[name].toString()
    };
    messages.push(node);

    // If the timesOfChange vector is uninitialized, we'll start
    // it at 1 if the statistics value is non-zero.  This is not always
    // correct because some stats might shrink back to zero, but this
    // number-of-changes-count is an estimation anyway, and I believe this
    // produces a more useful sort-by-number-of-changes function.
    var newValue = variables[name];
    var prevChangeCount = this.timesOfChange_[trimmedName];
    if (prevChangeCount == undefined) {
      if (newValue) {           // Not undefined, and not 0.
        this.timesOfChange_[trimmedName] = 1;
      } else {
        this.timesOfChange_[trimmedName] = 0;
      }
    } else if (newValue != this.lastValue_[trimmedName]) {
      this.timesOfChange_[trimmedName] = prevChangeCount + 1;
    }
    this.lastValue_[trimmedName] = newValue;
  }
  this.psolMessages_ = messages;
  this.numUnfilteredMessages_ = messages.length;
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
  // Ignore the trailing '/'.
  var n = pathName.lastIndexOf('/', pathName.length - 2);
  // e.g. /pagespeed_admin/foo or pagespeed_admin/foo
  if (n > 0) {
    return pathName.substring(0, n) + '/stats_json';
  } else {
    // e.g. /pagespeed_admin or pagespeed_admin
    return pathName + '/stats_json';
  }
};


/**
 * Refreshes the page by making requsts to server.
 */
pagespeed.Statistics.prototype.performRefresh = function() {
  if (!this.xhr_.isActive()) {
    if (!this.firstRefreshStarted_ || this.autoRefresh_) {
      this.firstRefreshStarted_ = true;
      this.xhr_.send(this.requestUrl());
    }
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
    setInterval(goog.bind(statisticsObj.performRefresh, statisticsObj),
                pagespeed.Statistics.FREQUENCY_);
    goog.events.listen(
        statisticsObj.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(statisticsObj.parseAjaxResponse, statisticsObj));
    statisticsObj.performRefresh();
  };
  goog.events.listen(window, 'load', statisticsOnload);
};
