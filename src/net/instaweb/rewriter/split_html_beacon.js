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
 * @fileoverview Code for detecting and sending to server the critical images
 * (images above the fold) on the client side.
 *
 * @author jud@google.com (Jud Porter)
 */

goog.require('pagespeedutils');
goog.require('pagespeedutils.CriticalXPaths');

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options. This is required
 *     to perform the property cache lookup when the beacon is handled by the
 *     sever.
 * @param {string} nonce The nonce set by the server.
 */
pagespeed.SplitHtmlBeacon = function(beaconUrl, htmlUrl, optionsHash, nonce) {
  /**
   * List of xpath pairs in the form start_xpath:end_xpath.
   */
  this.xpathPairs = [];

  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.windowSize_ = pagespeedutils.getWindowSize();
  this.imgLocations_ = {};
};

/**
 * Check position of images and input tags and beacon back images that are
 * visible on initial page load.
 * @private
 */
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  // Define the maximum size of a POST that the server will accept. We shouldn't
  // send more data than this.
  // TODO(jud): Factor out this const so that it matches kMaxPostSizeBytes.
  var MAX_DATA_LEN = 131072;

  var criticalXPaths = new pagespeedutils.CriticalXPaths(
      this.windowSize_.width, this.windowSize_.height, document);
  this.xpathPairs = criticalXPaths.getNonCriticalPanelXPathPairs();

  if (this.xpathPairs.length != 0) {
    var data = 'oh=' + this.optionsHash_;
    data += '&xp=' + encodeURIComponent(this.xpathPairs[0]);
    for (var i = 1; i < this.xpathPairs.length; ++i) {
      var tmp = ',' + encodeURIComponent(this.xpathPairs[i]);
      // TODO(jud): Currently we just truncate the data if it exceeds our
      // maximum POST request size limit. Check how large typical XPath sets
      // are, and if they are approaching this limit, either raise the limit or
      // split up the POST into multiple requests.
      if ((data.length + tmp.length) > MAX_DATA_LEN) {
        break;
      }
      data += tmp;
    }
    // Export the URL for testing purposes.
    pagespeed['splitHtmlBeaconData'] = data;
    // TODO(jud): This beacon should coordinate with the add_instrumentation JS
    // so that only one beacon request is sent if both filters are enabled.
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data);
  }
};

/**
 * Initialize.
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options.
 * @param {string} nonce The nonce set by the server.
 */
pagespeed.splitHtmlBeaconInit = function(beaconUrl, htmlUrl, optionsHash,
                                         nonce) {
  var temp = new pagespeed.SplitHtmlBeacon(
      beaconUrl, htmlUrl, optionsHash, nonce);
  // Add event to the onload handler to scan images and beacon back the visible
  // ones.
  var beacon_onload = function() {
    temp.checkSplitHtml_();
  };
  pagespeedutils.addHandler(window, 'load', beacon_onload);
};

pagespeed['splitHtmlBeaconInit'] = pagespeed.splitHtmlBeaconInit;
