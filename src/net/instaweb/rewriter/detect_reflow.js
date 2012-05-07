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
 * @fileoverview Detecting reflows due to deferred js execution.  To be used in
 * conjunction with js_defer.js.
 *
 * @author sriharis@google.com (Srihari Sukumaran)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 */
pagespeed.DetectReflow = function() {
  /**
   * Height of divs before scripts executed.
   * @type {!Object.<string, number>}
   * @private
   */
  this.preScriptHeights_ = {};

  /**
   * String that contains a comma separated list of "div:height", where height
   * represents the final rendered height after scripts executed.
   * @type {!string}
   * @private
   */
  this.reflowElementHeights_ = '';

  /**
   * Flag to indicate that deferred script execution is done.
   * @type {!boolean}
   * @private
   */
  this.jsDeferDone_ = false;
};

/**
 * Returns preScriptHeights_ object.
 * @return {!Object.<string, number>} Height of divs before scripts executed.
 */
pagespeed.DetectReflow.prototype.getPreScriptHeights = function() {
  return this.preScriptHeights_;
};
pagespeed.DetectReflow.prototype['getPreScriptHeights'] =
  pagespeed.DetectReflow.prototype.getPreScriptHeights;

/**
 * Returns reflowElementHeights_ string.
 * @return {!string}  A comma separated list of "div:height", where height
 * represents the final rendered height after scripts executed.
 */
pagespeed.DetectReflow.prototype.getReflowElementHeight = function() {
  return this.reflowElementHeights_;
};
pagespeed.DetectReflow.prototype['getReflowElementHeight'] =
  pagespeed.DetectReflow.prototype.getReflowElementHeight;

/**
 * Returns jsDeferDone_ boolean.
 * [google]  The webkit_headless extension js checks this before extracting
 * [google]  the onloadcomplete heights of divs.
 * @return {!boolean}  Returns true if reflow detection is done.
 */
pagespeed.DetectReflow.prototype.isJsDeferDone = function() {
  return this.jsDeferDone_;
};
pagespeed.DetectReflow.prototype['isJsDeferDone'] =
  pagespeed.DetectReflow.prototype.isJsDeferDone;

/**
 * Global singleton instance of DetectReflow.
 */
pagespeed.detectReflow = new pagespeed.DetectReflow();
pagespeed['detectReflow'] = pagespeed.detectReflow;

/**
 * For all divs with attribute 'id', adds a mapping from div 'id' to
 * clientHeight in preScriptHeights_.
 */
pagespeed.labelDivsBeforeDeferredJs = function() {
  var divs = document.getElementsByTagName('div');
  var len = divs.length;
  for (var i = 0; i < len; ++i) {
    var div = divs[i];
    if (div.hasAttribute('id') && div.clientHeight != undefined) {
      pagespeed.detectReflow.preScriptHeights_[div.getAttribute('id')] =
          div.clientHeight;
    }
    // TODO(sriharis):  How to handle divs with 'class' and no 'id'?
  }
};

/**
 * For all divs in preScriptHeights_, if the clientHeight now is different from
 * the div's height mapped in preScriptHeights_ then '<div id>:<current height>'
 * is appended to reflowElementHeights_.
 * [google]  This is not used in the webkit_headless flow in which this filter
 * [google]  is actually applied.  It is here for debugging.
 */
pagespeed.findChangedDivsAfterDeferredJs = function() {
  if (!pagespeed.detectReflow.preScriptHeights_ && console) {
    console.log('pagespeed.detectReflow.preScriptHeights_ is not available');
  }
  // Check reflow
  var newDivs = document.getElementsByTagName('div');
  var newLen = newDivs.length;
  for (var i = 0; i < newLen; ++i) {
    var div = newDivs[i];
    if (!div.hasAttribute('id')) {
      continue;
    }
    var divId = div.getAttribute('id');
    if (pagespeed.detectReflow.preScriptHeights_[divId] != undefined) {
      if (pagespeed.detectReflow.preScriptHeights_[divId] != div.clientHeight) {
        pagespeed.detectReflow.reflowElementHeights_ =
            pagespeed.detectReflow.reflowElementHeights_ + divId + ':' +
            window.getComputedStyle(div, null).getPropertyValue('height') + ',';
      }
    }
  }
};

/**
 * To be run as an 'AfterDeferRun' hook in js_defer.js.  Sets the jsDeferDone_
 * flag to true.
 */
pagespeed.setJsDeferDone = function() {
  pagespeed.detectReflow.jsDeferDone_ = true;
};

// TODO(sriharis):  Make labelDivsBeforeDeferredJs and
// setJsDeferDone prototype functions and change js_defer.js so that
// addBeforeDeferRunFunctions and addAfterDeferRunFunctions takes a function and
// on object to call the function on.

if (pagespeed.deferJs != 'undefined' &&
    pagespeed.deferJs['addBeforeDeferRunFunctions'] != 'undefined' &&
    pagespeed.deferJs['addAfterDeferRunFunctions'] != 'undefined') {
  pagespeed.deferJs['addBeforeDeferRunFunctions'](
      pagespeed.labelDivsBeforeDeferredJs);
  pagespeed.deferJs['addAfterDeferRunFunctions'](
      pagespeed.setJsDeferDone);
}
