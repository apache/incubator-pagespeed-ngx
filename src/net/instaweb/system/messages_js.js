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
 * @fileoverview Code for adding reversing lists, auto-refresh and filtering
 * functionalities to message history page in admin site.

 * @author oschaaf@google.com (Otto van der Schaaf)
 * @author xqyin@google.com (XiaoQian Yin)
 */

goog.provide('pagespeed.Messages');

goog.require('goog.events');
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.require('goog.string');
goog.require('pagespeedutils');



/**
 * @constructor
 * @param {goog.testing.net.XhrIo=} opt_xhr
 *     Optional mock XmlHttpRequests handler for testing.
 */
pagespeed.Messages = function(opt_xhr) {
  /**
   * The XmlHttpRequests handler for auto-refresh. We pass a mock XhrIo
   * when testing. Default is a normal XhrIo.
   * @private {goog.net.XhrIo|goog.testing.net.XhrIo}
   */
  this.xhr_ = opt_xhr || new goog.net.XhrIo();

  /**
   * The messages to be shown on the page.
   * @private {!Array.<string>}
   */
  this.psolMessages_ = document.getElementById('log').innerHTML.split('\n');

  /**
   * The flag to indicate the refreshing status.
   * @private {boolean}
   */
  this.isRefreshing_ = false;

  /**
   * The option of reversing list.
   * @private {boolean}
   */
  this.reverse_ = false;

  /**
   * The option of filtering.
   * @private {string}
   */
  this.filter_ = '';

  /**
   * The option of auto-refresh.
   * @private {boolean}
   */
  this.autoRefresh_ = false;

  var logElement = document.getElementById('log');
  var uiTable = document.createElement('div');
  uiTable.innerHTML = this.htmlString();
  document.body.insertBefore(uiTable, logElement);
};


/**
 * Updates the option of reversing the messages list.
 */
pagespeed.Messages.prototype.toggleReverse = function() {
  this.reverse_ = !this.reverse_;
  this.update();
};


/**
 * Updates the option of auto-refresh.
 */
pagespeed.Messages.prototype.toggleAutorefresh = function() {
  this.autoRefresh_ = !this.autoRefresh_;
};


/**
 * Updates the option of messages filter.
 * @param {HTMLElement} element The HTML element of Filter.
 */
pagespeed.Messages.prototype.toggleFilter = function(element) {
  this.filter_ = element.value;
  this.update();
};


/**
 * Reverses or filters messages according to the options.
 */
pagespeed.Messages.prototype.update = function() {
  var logElement = document.getElementById('log');
  var messages = this.psolMessages_.slice(0);
  if (this.filter_) {
    for (var i = messages.length - 1; i >= 0; --i) {
      if (!messages[i] ||
          !goog.string.caseInsensitiveContains(messages[i], this.filter_)) {
        messages.splice(i, 1);
      }
    }
  }
  if (messages.length < 2) {
    logElement.innerHTML = messages;
  } else {
    // The messages here are already escaped in C++.
    if (this.reverse_) {
      logElement.innerHTML = messages.reverse().join('\n');
    } else {
      logElement.innerHTML = messages.join('\n');
    }
  }
};


/**
  * The error message of dump failure.
  * @private {string}
  * @const
  */
pagespeed.Messages.DUMP_ERROR_ =
    '<pre>Failed to write messages to this page. ' +
    'Please check pagespeed.conf to see if it\'s enabled.</pre>\n';


/**
 * Parses new messages from the server response.
 * @param {string} text The raw text content reponsed by server.
 * The expected format of text should be like this:
 * <html>
 *   <head>...</head>
 *   <body>
 *     <div style=...>...</div><hr>
 *     <div>...</div>
 *     <div id="log">...</div>
 *     <script type="text/javascript">...</script>
 *   </body>
 * </html>
 * @return {!Array.<string>} messages The updated messages list.
 */
pagespeed.Messages.prototype.parseMessagesFromResponse = function(text) {
  var messages = [];
  // TODO(xqyin): Add a handler method in AdminSite to provide proper XHR
  // response instead of depending on the format like this.
  var start = text.indexOf('<div id=\"log\">');
  var end = text.indexOf('<script type=\'text/javascript\'>', start);
  if (start >= 0 && end >= 0) {
    start = start + '<div id=\"log\">'.length;
    end = end - '</div>\n'.length;
    messages = text.substring(start, end).split('\n');
  } else {
    messages.push(pagespeed.Messages.DUMP_ERROR_);
  }
  return messages;
};


/**
  * The error message of auto-refresh.
  * @private {string}
  * @const
  */
pagespeed.Messages.REFRESH_ERROR_ =
    '<pre>Sorry, the message history cannot be loaded. ' +
    'Please wait and try again later.</pre>\n';


/**
 * Refreshs the page by making requsts to server.
 */
pagespeed.Messages.prototype.autoRefresh = function() {
  if (this.autoRefresh_ && !this.isRefreshing_) {
    this.isRefreshing_ = true;
    var fetchContent = function(messagesObj) {
      if (this.isSuccess()) {
        var newText = this.getResponseText();
        messagesObj.psolMessages_ =
            messagesObj.parseMessagesFromResponse(newText);
        messagesObj.update();
      } else {
        console.log(this.getLastError());
        document.getElementById('log').innerHTML =
            pagespeed.Messages.REFRESH_ERROR_;
      }
      messagesObj.isRefreshing_ = false;
    };
    goog.events.listen(
        this.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(fetchContent, this.xhr_, this));
    this.xhr_.send(document.location.href);
  }
};


/**
 * Generates the HTML code to inject UI to the page.
 * @return {string}
 */
pagespeed.Messages.prototype.htmlString = function() {
  // TODO(xqyin): Use DOM functions to inject the UI table.
  var html = '';
  html += '<table border=1 style=\'border-collapse: ';
  html += 'collapse;border-color:silver;\'><tr valign=\'center\'>';
  html += '<td>Reverse: <input type=\'checkbox\' id=\'reverse\' ';
  html += (this.reverse_ ? 'checked' : '') + '></td>';
  html += '<td>Auto refresh: <input type=\'checkbox\' id=\'autoRefresh\' ';
  html += (this.autoRefresh_ ? 'checked' : '') + '></td>';
  html += '<td>&nbsp;&nbsp;&nbsp;&nbsp;Search: ';
  html += '<input id=\'txtFilter\' type=\'text\'></td>';
  html += '</tr></table>';
  return html;
};


/**
 * The Main entry to start processing.
 * @export
 */
pagespeed.Messages.Start = function() {
  var messagesOnload = function() {
    var messagesObj = new pagespeed.Messages();
    var filterElement = document.getElementById('txtFilter');
    goog.events.listen(
        filterElement, 'keyup', goog.bind(messagesObj.toggleFilter,
                                          messagesObj, filterElement));
    goog.events.listen(document.getElementById('reverse'), 'change',
                       goog.bind(messagesObj.toggleReverse, messagesObj));
    goog.events.listen(document.getElementById('autoRefresh'), 'change',
                       goog.bind(messagesObj.toggleAutorefresh, messagesObj));
    messagesObj.update();
    setInterval(messagesObj.autoRefresh.bind(messagesObj), 5000);
  };
  pagespeedutils.addHandler(window, 'load', messagesOnload);
};
