/*
 * Copyright 2011 Google Inc.
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
 * @fileoverview Code for deferring javascript on client side.
 * This javascript is part of JsDefer filter.
 *
 * @author atulvasu@google.com (Atul Vasu)
 */

var pagespeed = pagespeed || {};

/**
 * @constructor
 */
pagespeed.DeferJs = function() {
  /**
   * @type {Array.<function()>}
   * @private
   */
  this.queue_ = [];

  /**
   * @type {Array.<string>}
   * @export
   */
  this.logs = [];

  /**
   * @type {number}
   * @private
   */
  this.next_ = 0;
};

/**
 * Add to defer_logs if logs are enabled.
 * @param {!string} line line to be added to log.
 */
pagespeed.DeferJs.prototype.log = function(line) {
  if (this.logs) {
    this.logs.push('' + line);
  }
};

/**
 * Defers execution of 'str', by adding it to the queue.
 * @param {string} str valid javascript snippet.
 */
pagespeed.DeferJs.prototype.addStr = function(str) {
  var me = this; // capture closure.
  this.queue_.push(function() {
    window.eval(str);
    me.log('Evaluated: ' + str);
    // TODO(atulvasu): Detach stack here to prevent recursion issues.
    me.runNext();
  });
};

/**
 * Defers execution of contents of 'url'.
 * @param {string} url returns javascript when fetched.
 */
pagespeed.DeferJs.prototype.addUrl = function(url) {
  var me = this; // capture closure.
  this.queue_.push(function() {
    var script = document.createElement('script');
    script.setAttribute('src', url);
    script.setAttribute('type', 'text/javascript');
    pagespeed.addOnload(script, function() {
      me.log('Executed: ' + url);
      me.runNext();
    });
    document.body.appendChild(script);
  });
};

/**
 * Schedules the next task in the queue.
 */
pagespeed.DeferJs.prototype.runNext = function() {
  if (this.next_ < this.queue_.length) {
    // Done here to prevent another _run_next() in stack from
    // seeing the same value of next, and get into infinite
    // loop.
    this.next_++;
    try {
      this.queue_[this.next_ - 1].call(window);
    } catch (err) {
    }
  }
};

/**
 * Starts the execution of all the deferred scripts.
 */
pagespeed.DeferJs.prototype.run = function() {
  this.runNext();
};

/**
 * Runs the function when page is loaded.
 * @param {Element} elem Element to attach handler.
 * @param {function()} func New onload handler.
 */
pagespeed.addOnload = function(elem, func) {
  if (elem.addEventListener) {
    elem.addEventListener('load', func, false);
  } else if (window.attachEvent) {
    elem.attachEvent('onload', func);
  } else {
    var oldHandler = elem.onload;
    elem.onload = function() {
      func.call(this);
      if (oldHandler) {
        oldHandler.call(this);
      }
    }
  }
};

/**
 * Initialize defer javascript.
 */
pagespeed.deferInit = function() {
  pagespeed.deferJs = new pagespeed.DeferJs();
  pagespeed.addOnload(window, function() {
    pagespeed.deferJs.run();
  });
};
