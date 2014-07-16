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
  this.psolMessages_ = document.getElementById('stat').innerText.split('\n');

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

  // The UI table of auto-refresh and filtering.
  var uiTable = document.createElement('div');
  uiTable.id = 'uiDiv';
  uiTable.innerHTML =
      '<table id="uiTable" border=1 style="border-collapse: ' +
      'collapse;border-color:silver;"><tr valign="center">' +
      '<td>Auto refresh: <input type="checkbox" id="autoRefresh" ' +
      (this.autoRefresh_ ? 'checked' : '') + '></td>' +
      '<td>&nbsp;&nbsp;&nbsp;&nbsp;Search: ' +
      '<input id="txtFilter" type="text"></td>' +
      '</tr></table>';
  document.body.insertBefore(uiTable, document.getElementById('stat'));
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
 * Filters and displays updated statistics.
 */
pagespeed.Statistics.prototype.update = function() {
  var statElement = document.getElementById('stat');
  var messages = goog.array.clone(this.psolMessages_);
  if (this.filter_) {
    for (var i = messages.length - 1; i >= 0; --i) {
      if (!messages[i] ||
          !goog.string.caseInsensitiveContains(messages[i], this.filter_)) {
        messages.splice(i, 1);
      }
    }
  }
  statElement.innerText = messages.join('\n');
};


/**
  * The error message of dump failure.
  * @private {string}
  * @const
  */
pagespeed.Statistics.DUMP_ERROR_ =
    'Sorry, failed to write statistics to this page.';


/**
 * Parses new statistics from the server response.
 * @param {string} text The raw text content sent by server.
 * The expected format of text should be like this:
 * <html>
 *   <head>...</head>
 *   <body>
 *     <div style=...>...</div><hr>
 *     <div id="uiDiv">...</div>
 *     <pre id="stat">...</pre>
 *     <script type="text/javascript">...</script>
 *   </body>
 * </html>
 * @return {!Array.<string>} messages The updated statistics list.
 */
pagespeed.Statistics.prototype.parseMessagesFromResponse = function(text) {
  var messages = [];
  // TODO(xqyin): Add a handler method in AdminSite to provide proper XHR
  // response instead of depending on the format like this.
  var start = text.indexOf('<pre id="stat">');
  var end = text.indexOf('</pre>', start);
  if (start >= 0 && end >= 0) {
    start = start + '<pre id="stat">'.length;
    end = end - 1;
    messages = text.substring(start, end).split('\n');
  } else {
    messages.push(pagespeed.Statistics.DUMP_ERROR_);
  }
  return messages;
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
 * Refreshes the page by making requsts to server.
 */
pagespeed.Statistics.prototype.performRefresh = function() {
  if (!this.xhr_.isActive() && this.autoRefresh_) {
    var fetchContent = function(statisticsObj) {
      if (this.isSuccess()) {
        var newText = this.getResponseText();
        statisticsObj.psolMessages_ =
            statisticsObj.parseMessagesFromResponse(newText);
        statisticsObj.update();
      } else {
        console.log(this.getLastError());
        document.getElementById('stat').innerText =
            pagespeed.Statistics.REFRESH_ERROR_;
      }
    };
    goog.events.listen(
        this.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(fetchContent, this.xhr_, this));
    this.xhr_.send(document.location.href);
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
    var filterElement = document.getElementById('txtFilter');
    goog.events.listen(
        filterElement, 'keyup', goog.bind(statisticsObj.setFilter,
                                          statisticsObj, filterElement));
    goog.events.listen(document.getElementById('autoRefresh'), 'change',
                       goog.bind(statisticsObj.toggleAutorefresh,
                                 statisticsObj));
    setInterval(statisticsObj.performRefresh.bind(statisticsObj),
                pagespeed.Statistics.FREQUENCY_);
  };
  goog.events.listen(window, 'load', statisticsOnload);
};
