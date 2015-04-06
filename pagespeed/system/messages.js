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
 *
 * @author oschaaf@we-amp.com (Otto van der Schaaf)
 * @author xqyin@google.com (XiaoQian Yin)
 */

goog.provide('pagespeed.Messages');

goog.require('goog.array');
goog.require('goog.events');
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.require('goog.string');



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
  // Note that the element with id 'log' must exist. Otherwise the JS won't
  // be written to the sources.
  // The last entry of the messages array is empty because there is a '\n'
  // at the end.
  if (this.psolMessages_.length > 0) {
    this.psolMessages_.pop();
  }

  /**
   * The total number of unfiltered messages.
   * @private {number}
   */
  this.numUnfilteredMessages_ = this.psolMessages_.length;

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

  var wrapper = document.createElement('div');
  wrapper.style.overflow = 'hidden';
  wrapper.style.clear = 'both';

  var uiTable = document.createElement('div');
  uiTable.id = 'ui-div';
  uiTable.innerHTML =
      '<table id="ui-table" border="1" style="float:left; border-collapse: ' +
      'collapse;border-color:silver;"><tr valign="center">' +
      '<td>Reverse: <input type="checkbox" id="reverse" ' +
      (this.reverse_ ? 'checked' : '') + '></td>' +
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

  var logElement = document.getElementById('log');
  document.body.insertBefore(wrapper, logElement);

  this.updateMessageCount();
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
pagespeed.Messages.prototype.setFilter = function(element) {
  this.filter_ = element.value;
  this.update();
};


/**
 * Update the text showing the number of messages.
 * @param {number=} opt_num An optional specified number to show when
 *     filtering the messages.
 */
pagespeed.Messages.prototype.updateMessageCount = function(opt_num) {
  // The default number of messages is the length of the array when opt_num
  // is not specified.
  var total = (opt_num != undefined) ? opt_num : this.psolMessages_.length;
  if (total == this.numUnfilteredMessages_) {
    document.getElementById('num').textContent = 'Total message count: ' +
        total;
  } else {
    document.getElementById('num').textContent =
        'Visible message count: ' + total + '/' + this.numUnfilteredMessages_;
  }
};


/**
 * Reverses or filters messages according to the options.
 */
pagespeed.Messages.prototype.update = function() {
  var logElement = document.getElementById('log');
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
  // The messages here are already escaped in C++.
  if (this.reverse_) {
    logElement.innerHTML = messages.reverse().join('\n');
  } else {
    logElement.innerHTML = messages.join('\n');
  }
};


/**
  * The error message of dump failure.
  * @private {string}
  * @const
  */
pagespeed.Messages.DUMP_ERROR_ =
    'Failed to write messages to this page. ' +
    'Verify that MessageBufferSize is not set to 0 in pagespeed.conf.';


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
 */
pagespeed.Messages.prototype.parseMessagesFromResponse = function(text) {
  var messages = [];
  // TODO(xqyin): Add a handler method in AdminSite to provide proper XHR
  // response instead of depending on the format like this.
  var start = text.indexOf('<div id="log">');
  var end = text.indexOf('<script type="text/javascript">', start);
  if (start >= 0 && end >= 0) {
    start = start + '<div id="log">'.length;
    end = end - '</div>\n'.length;
    messages = text.substring(start, end).split('\n');
    messages.pop();
    this.psolMessages_ = messages;
    this.numUnfilteredMessages_ = messages.length;
    this.update();
  } else {
    goog.array.clear(this.psolMessages_);
    this.numUnfilteredMessages_ = 0;
    this.updateMessageCount();
    document.getElementById('log').textContent = pagespeed.Messages.DUMP_ERROR_;
  }
};


/**
  * The error message of auto-refresh.
  * @private {string}
  * @const
  */
pagespeed.Messages.REFRESH_ERROR_ =
    'Sorry, the message history cannot be loaded. ' +
    'Please wait and try again later.';


/**
 * Refreshs the page by making requsts to server.
 */
pagespeed.Messages.prototype.autoRefresh = function() {
  if (this.autoRefresh_ && !this.xhr_.isActive()) {
    this.xhr_.send(document.location.href);
  }
};


/**
 * Parse the response sent by server.
 */
pagespeed.Messages.prototype.parseAjaxResponse = function() {
  if (this.xhr_.isSuccess()) {
    var newText = this.xhr_.getResponseText();
    this.parseMessagesFromResponse(newText);
  } else {
    console.log(this.xhr_.getLastError());
    goog.array.clear(this.psolMessages_);
    this.numUnfilteredMessages_ = 0;
    this.updateMessageCount();
    document.getElementById('log').textContent =
        pagespeed.Messages.REFRESH_ERROR_;
  }
};


/**
 * The Main entry to start processing.
 * @export
 */
pagespeed.Messages.Start = function() {
  var messagesOnload = function() {
    var messagesObj = new pagespeed.Messages();
    var filterElement = document.getElementById('text-filter');
    goog.events.listen(
        filterElement, 'keyup', goog.bind(messagesObj.setFilter,
                                          messagesObj, filterElement));
    goog.events.listen(document.getElementById('reverse'), 'change',
                       goog.bind(messagesObj.toggleReverse, messagesObj));
    goog.events.listen(document.getElementById('auto-refresh'), 'change',
                       goog.bind(messagesObj.toggleAutorefresh, messagesObj));
    goog.events.listen(messagesObj.xhr_, goog.net.EventType.COMPLETE,
                       goog.bind(messagesObj.parseAjaxResponse, messagesObj));
    setInterval(goog.bind(messagesObj.autoRefresh, messagesObj), 5000);
  };
  goog.events.listen(window, 'load', messagesOnload);
};
