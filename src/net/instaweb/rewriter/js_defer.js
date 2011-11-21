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
}

/**
 * Defers execution of 'str', by adding it to the queue.
 * @param {string} str valid javascript snippet.
 */
pagespeed.DeferJs.prototype.addStr = function(str) {
  this.queue_.push(function() {
    window.eval(str);
  });
};

/**
 * Defers execution of contents of 'url'.
 * @param {string} url returns javascript when fetched.
 */
pagespeed.DeferJs.prototype.addUrl = function(url) {
  this.queue_.push(function() {
    var script = document.createElement('script');
    script.setAttribute('src', url);
    script.setAttribute('type', 'text/javascript');
    document.body.appendChild(script);
  });
};

/**
 * Executes all the deferred scripts.
 */
pagespeed.DeferJs.prototype.run = function() {
  var len = this.queue_.length;
  for (var i = 0; i < len; i++) {
    try {
      this.queue_[i].call(window);
    } catch (err) {}
  }
};

/**
 * Runs the function when page is loaded.
 * @param {function()} func New onload handler.
 */
pagespeed.addOnload = function(func) {
  if (window.addEventListener) {
    window.addEventListener('load', func, false);
  } else if (window.attachEvent) {
    window.attachEvent('onload', func);
  } else {
    var oldHandler = window.onload;
    window.onload = function() {
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
  pagespeed.addOnload(function() {
    pagespeed.deferJs.run();
  });
};
