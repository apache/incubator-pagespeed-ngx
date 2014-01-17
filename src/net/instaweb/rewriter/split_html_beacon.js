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
  // XPaths of below the fold nodes.
  this.btfNodes_ = [];

  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.windowSize_ = pagespeedutils.getWindowSize();
};


/**
 * Walk the DOM recursively, generating the list of below-the-fold node XPaths
 * as we go. A node is added to btfNodes if it is below-the-fold, and its parent
 * or one of its siblings is not. This logic depends on the property/belief that
 * for a node to be considered below-the-fold either it must not be in the
 * unscrolled viewport and must either be a leaf node or all of its descendants
 * must be considered below-the-fold.
 * @param {Node} node Node to check. document.body should be passed in on the
 *     first call.
 * @return {boolean} Returns true if the node (and all of its children) is BTF.
 *     Otherwise, all BTF children of node are added to btfNodes and false is
 *     returned.
 * @private
 */
pagespeed.SplitHtmlBeacon.prototype.walkDom_ = function(node) {
  var allChildrenBtf = true;
  var btfChildren = [];
  for (var currChild = node.firstChild; currChild != null;
       currChild = currChild.nextSibling) {
    if (currChild.nodeType !== Node.ELEMENT_NODE ||
        currChild.tagName == 'SCRIPT' ||
        currChild.tagName == 'NOSCRIPT' ||
        currChild.tagName == 'STYLE' ||
        currChild.tagName == 'LINK') {
      continue;
    }
    if (this.walkDom_(currChild)) {
      btfChildren.push(currChild);
    } else {
      allChildrenBtf = false;
    }
  }

  if (allChildrenBtf && !pagespeedutils.inViewport(node, this.windowSize_)) {
    return true;
  }

  for (var i = 0; i < btfChildren.length; ++i) {
    this.btfNodes_.push(pagespeedutils.generateXPath(
        btfChildren[i], window.document));
  }

  return false;
};


/**
 * Check position of HTML elements and beacon back below-the-fold elements.
 * @private
 */
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  this.walkDom_(document.body);

  if (this.btfNodes_.length != 0) {
    var data = 'oh=' + this.optionsHash_ + '&n=' + this.nonce_;
    data += '&xp=' + encodeURIComponent(this.btfNodes_[0]);
    for (var i = 1; i < this.btfNodes_.length; ++i) {
      var tmp = ',' + encodeURIComponent(this.btfNodes_[i]);
      // TODO(jud): Currently we just truncate the data if it exceeds our
      // maximum POST request size limit. Check how large typical XPath sets
      // are, and if they are approaching this limit, either raise the limit or
      // split up the POST into multiple requests.
      if ((data.length + tmp.length) > pagespeedutils.MAX_POST_SIZE) {
        break;
      }
      data += tmp;
    }
    // TODO(jud): Coordinate with other beacons so that only 1 request needs to
    // be sent.
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
  var beaconOnload = function() {
    temp.checkSplitHtml_();
  };
  pagespeedutils.addHandler(window, 'load', beaconOnload);
};

pagespeed['splitHtmlBeaconInit'] = pagespeed.splitHtmlBeaconInit;
