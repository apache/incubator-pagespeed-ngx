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

/** @type {Array.<function()>} */
pagespeed.defer_queue = [];

/**
 * Defers execution of 'str'.
 * @param {string} str valid javascript snippet.
 */
pagespeed.defer_str = function(str) {
  pagespeed.defer_queue.push(function() {
    window.eval(str);
  });
};

/**
 * Defers execution of contents of 'url'.
 * @param {string} url returns javascript when fetched.
 */
pagespeed.defer_url = function(url) {
  pagespeed.defer_queue.push(function() {
      var script = document.createElement('script');
      script.setAttribute('src', url);
      script.setAttribute('type', 'text/javascript');
      document.body.appendChild(script);
  });
};

/**
 * Executes all the deferred scripts.
 */
pagespeed.defer_run = function() {
  var len = pagespeed.defer_queue.length;
  for (var i = 0; i < len; i++) {
    try {
      pagespeed.defer_queue[i].call(window);
    } catch (err) {}
  }
};

/** Setting the window onload. */
window.onload = pagespeed.defer_run;
