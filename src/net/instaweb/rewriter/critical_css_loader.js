/*
 * Copyright 2013 Google Inc.
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
 * @fileoverview Lazyloads the full CSS that was delayed by the
 * CriticalSelectorFilter. This script will not work on IE8, but the
 * CriticalSelectorFilter is disabled for IE anyways.
 */

goog.provide('pagespeed.CriticalCssLoader');

goog.require('pagespeedutils');


/** @private {boolean} */
pagespeed.CriticalCssLoader.stylesAdded_ = false;


/**
 * Loads deferred CSS in noscript tags by copying the text of the noscript into
 * a new div element.
 */
pagespeed.CriticalCssLoader.addAllStyles = function() {
  if (pagespeed.CriticalCssLoader.stylesAdded_) { return; }
  pagespeed.CriticalCssLoader.stylesAdded_ = true;

  var elements = document.getElementsByClassName('psa_add_styles');

  for (var i = 0, e; e = elements[i]; ++i) {
    if (e.nodeName != 'NOSCRIPT') { continue; }
    var div = document.createElement('div');
    div.innerHTML = e.textContent;
    document.body.appendChild(div);
  }
};


/**
 * Sets up the CSS style lazyloader to run at the appropriate event. Runs at
 * requestAnimationFrame if it is available, since that will ensure the page has
 * rendered before loading the CSS. Otherwise, wait until onload.
 * @export
 */
pagespeed.CriticalCssLoader.Run = function() {
  var raf = pagespeedutils.getRequestAnimationFrame();
  if (raf) {
    raf(function() {
      window.setTimeout(pagespeed.CriticalCssLoader.addAllStyles, 0);
    });
  } else {
    pagespeedutils.addHandler(
        window, 'load', pagespeed.CriticalCssLoader.addAllStyles);
  }
};
