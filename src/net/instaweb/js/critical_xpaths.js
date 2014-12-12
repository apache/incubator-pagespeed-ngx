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
 * @fileoverview JS class for finding below-the-fold XPaths used by split_html.
 */

goog.provide('pagespeedutils.CriticalXPaths');
goog.provide('pagespeedutils.generateXPath');

goog.require('pagespeedutils');



/**
 * @constructor
 * @param {number} viewportWidth Width of the viewport.
 * @param {number} viewportHeight Height of the viewport.
 * @param {Node} document The root document.
 */
pagespeedutils.CriticalXPaths = function(
    viewportWidth, viewportHeight, document) {
  /**
   * Viewport size.
   * @type {{ height: (number), width: (number) }}
   * @private
   */
  this.windowSize_ = {
    'height': viewportHeight,
    'width': viewportWidth
  };

  /**
   * List of xpath pairs in the form start_xpath:end_xpath.
   * @type {Array.<string>}
   * @private
   */
  this.xpathPairs_ = [];

  this.document_ = document;
};


/**
 * Gets the start and end xpath for the non-critical panels.
 * @return {Array.<string>}
 */
pagespeedutils.CriticalXPaths.prototype.getNonCriticalPanelXPathPairs =
    function() {
  // We only need to mark elements in the body. Elements in the head are never
  // visible.
  this.findNonCriticalPanelBoundaries_(this.document_.body);

  return this.xpathPairs_;
};


/**
 * Appends Appends a new XPath to the current list of XPath pairs.
 * @param {string} startXpath Start XPath.
 * @param {string} endXpath End XPath.
 * @private
 */
pagespeedutils.CriticalXPaths.prototype.addXPathPair_ = function(startXpath,
    endXpath) {
  if (!startXpath) {
    return;
  }
  var xpathPair = startXpath;
  if (endXpath) {
    xpathPair += ':' + endXpath;
  }
  this.xpathPairs_.push(xpathPair);
};


/**
 * Identifies all nodes which are not in viewport and populates the start and
 * end xpath of non-critical panels into xpathPairs_.
 * @param  {Node} node Dom node.
 * @return {boolean} true iff node in viewport or any of node's
 * descendants is critical.
 * @private
 */
pagespeedutils.CriticalXPaths.prototype.findNonCriticalPanelBoundaries_ =
    function(node) {
  var nodeIsCritical = pagespeedutils.inViewport(node, this.windowSize_);
  var prevChildIsCritical = nodeIsCritical;
  // The loop below determines the XPaths at the transition points of nodes
  // going from critical to non-critical. startChildXPath will be the first
  // non-critical node, and endChildXPath will be the first critical node after
  // the non-critical section.
  var startChildXPath = '';
  var endChildXPath = '';
  // Iterate over the visible (that is, not hidden, independent of viewport
  // size) sibling nodes of the passed in node.
  var firstChild = this.visibleNodeOrSibling_(node.firstChild);
  for (var currNode = firstChild; currNode != null;
       currNode = this.visibleNodeOrSibling_(currNode.nextSibling)) {
    // Check if we are at a criticality transition point by making a recursive
    // call to findNonCriticalPanelBoundaries for the current sibling node. If
    // we are at a transition point from critical to non-critical, we need to
    // mark the current sibling node as the start of an XPath. If we are
    // transitioning from non-critical to critical, then the current node is the
    // end of our XPath, and we need to store the complete path.
    // TODO(jud): The logic here is tricky to follow. See if we can simplify
    // this to make it clearer and more efficient.
    var currNodeIsCritical = this.findNonCriticalPanelBoundaries_(currNode);
    if (currNodeIsCritical != prevChildIsCritical) {
      if (!currNodeIsCritical) {
        // Going from critical -> non-critical.
        startChildXPath = pagespeedutils.generateXPath(
            currNode, this.document_);
        endChildXPath = '';
      } else {
        // Going from non-critical -> critical
        if (!nodeIsCritical) {
          // But non-critical panel never began.
          // All the nodes till currNode are non-critical.
          if (firstChild != currNode) {
            startChildXPath = pagespeedutils.generateXPath(
                firstChild, this.document_);
          }
          nodeIsCritical = true;
        }
        endChildXPath = pagespeedutils.generateXPath(currNode, this.document_);
        if (!endChildXPath) {
          // If 'endChildXPath' is empty here, then it is better not to split
          // here than treating few critical nodes as non-critical.
          startChildXPath = '';
        }
        if (startChildXPath) {
          this.addXPathPair_(startChildXPath, endChildXPath);
        }
        startChildXPath = '';
      }
      prevChildIsCritical = currNodeIsCritical;
    }
  }
  if (startChildXPath) {
    this.addXPathPair_(startChildXPath, endChildXPath);
  }
  return nodeIsCritical;
};


/**
 * Generates the xpath for the given node which is relative to body.
 * @param {Node} node Node in dom tree whose xpath is generated.
 * @param {Node} doc Page document.
 * @return {string} xpath of a node which is always a rendered one.
 */
pagespeedutils.generateXPath = function(node, doc) {
  var xpathUnits = [];
  while (node != doc.body) {
    var id = node.getAttribute('id');
    if (id && doc.querySelectorAll('#' + id).length == 1) {
      // Use 'id' inside xpath only if it is used once in the document.
      xpathUnits.unshift(node.tagName.toLowerCase() + '[@id=\"' +
          node.getAttribute('id') + '\"]');
      break;
    } else {
      var i = 0;
      for (var sibling = node;
           sibling;
           sibling = sibling.previousElementSibling) {
        // Please keep this list of tags same as that in
        // google_critical_line_info_finder.cc.
        if ((sibling.tagName === 'SCRIPT') ||
            (sibling.tagName === 'NOSCRIPT') ||
            (sibling.tagName === 'STYLE') ||
            (sibling.tagName === 'LINK')) {
          continue;
        }
        ++i;
      }
      xpathUnits.unshift(node.tagName.toLowerCase() + '[' + i + ']');
    }
    node = node.parentNode;
  }
  return xpathUnits.length ? xpathUnits.join('/') : '';
};


/**
 * @param {Node} node Node in DOM tree.
 * @return {?Node} Returns node if it is visible (as in width and height greater
 *     than 0, irrespective of the viewport), or the first visible sibling of
 *     node if it isn't. Returns null if no valid nodes are found.
 * @private
 */
pagespeedutils.CriticalXPaths.prototype.visibleNodeOrSibling_ = function(node) {
  for (; node != null; node = node.nextSibling) {
    if (node.nodeType != Node.ELEMENT_NODE) continue;
    if (node.offsetWidth == 0 && node.offsetHeight == 0) continue;
    // Also check node.offsetParent to see if the node is hidden. offsetParent
    // is null when the node has display:none or position:fixed styling, or if
    // it's a body node. To differentiate, we explicitly check for
    // position:fixed and body and do not consider those cases hidden.
    if (node.offsetParent || node.tagName == 'BODY' ||
        node.style.position == 'fixed') return node;
  }
  return null;
};
