/*
 * Copyright 2012 Google Inc.
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
 * @fileoverview Contains helper functions which will be needed by DeferIframe
 * filter.
 *
 * @author pulkitg@google.com (Pulkit Goyal)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 */
pagespeed.DeferIframe = function() {
};

/**
 * Convert first pagespeed_iframe element in the page to iframe.
 */
pagespeed.DeferIframe.prototype.convertToIframe = function() {
  var nodes = document.getElementsByTagName('pagespeed_iframe');
  if (nodes.length > 0) {
    var oldElement = nodes[0];
    var newElement = document.createElement('iframe');
    // Copy attributes.
    for (var i = 0, a = oldElement.attributes, n = a.length; i < n; ++i) {
      newElement.setAttribute(a[i].name, a[i].value);
    }
    // Replace the old element with the new one.
    oldElement.parentNode.replaceChild(newElement, oldElement);
  }
};
pagespeed.DeferIframe.prototype['convertToIframe'] =
  pagespeed.DeferIframe.prototype.convertToIframe;

/**
 * Initializes the defer iframe module.
 */
pagespeed.deferIframeInit = function() {
  var deferIframe = new pagespeed.DeferIframe();
  pagespeed['deferIframe'] = deferIframe;
};
pagespeed['deferIframeInit'] = pagespeed.deferIframeInit;
