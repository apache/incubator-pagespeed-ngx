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
 *
 * Author: jmarantz@google.com (Joshua Marantz)
 */


goog.provide('pagespeed.MobUtil');

goog.require('goog.dom');


/**
 * @fileoverview
 * Stateless utility functions for PageSped mobilization.
 */


/**
 * Returns true if the two rectangles intersect.
 * @param {!ClientRect} a A rectangle
 * @param {!ClientRect} b Another rectangle
 * @return {boolean}
 */
pagespeed.MobUtil.intersects = function(a, b) {
  return (a.left <= b.right &&
          b.left <= a.right &&
          a.top <= b.bottom &&
          b.top <= a.bottom);
};


/**
 * Takes a value, potentially ending in "px", and returns it as an
 * integer, or null.  Use 'if (return_value != null)' to validate the
 * return value of this function.  'if (return_value)' will not do what
 * you want if the value is 0.
 *
 * Note that this will return null if the dimensions are specified in a
 * units other than 'px'.  But if the dimensions are specified without any
 * units extension, this will assume pixels and return them as a number.
 *
 * @param {?string} value Attribute value.
 * @return {?number} integer value in pixels or null.
 */
pagespeed.MobUtil.pixelValue = function(value) {
  var ret = null;
  if (value) {
    var px = value.indexOf('px');
    if (px != -1) {
      value = value.substring(0, px);
    }
    ret = parseInt(value, 10);
    if (isNaN(ret)) {
      ret = null;
    }
  }
  return ret;
};


/**
 * Returns an integer pixel dimension or null.  Note that a null return
 * might mean the computed dimension is 'auto' or something.  This function
 * strips the literal "px" from the return value before parsing as an int.
 * @param {CSSStyleDeclaration} computedStyle The window.getComputedStyle of
 * an element.
 * @param {string} name The name of a CSS dimension.
 * @return {?number} the dimension value in pixels, or null if failure.
 */
pagespeed.MobUtil.computedDimension = function(computedStyle, name) {
  var value = null;
  if (computedStyle) {
    value = pagespeed.MobUtil.pixelValue(computedStyle.getPropertyValue(name));
  }
  return value;
};


/**
 * Removes a property from an HTML element.
 * @param {!Element} element The HTML DOM element.
 * @param {string} property The property to remove.
 */
pagespeed.MobUtil.removeProperty = function(element, property) {
  if (element.style) {
    element.style.removeProperty(property);
  }
  element.removeAttribute(property);
};


/**
 * Finds the dimension as requested directly on the object or its
 * immediate style.  Does not find dimensions on CSS classes, or
 * dimensions specified in 'em', percentages, or other units.
 *
 * @param {!Element} element The HTML DOM element.
 * @param {string} name The name of the dimension.
 * @return {?number} The pixel value as an integer, or null.
 */
pagespeed.MobUtil.findRequestedDimension = function(element, name) {
  // See if the value is specified in the style attribute.
  var value = null;
  if (element.style) {
    value = pagespeed.MobUtil.pixelValue(element.style.getPropertyValue(name));
  }

  if (value == null) {
    // See if the width is specified directly on the element.
    value = pagespeed.MobUtil.pixelValue(element.getAttribute(name));
  }

  return value;
};


/**
 * Sets a property in the element's style with a new value.  The new value
 * is written as '!important'.
 *
 * @param {!Element} element
 * @param {string} name
 * @param {string} value
 */
pagespeed.MobUtil.setPropertyImportant = function(element, name, value) {
  element.style.setProperty(name, value, 'important');
};


/**
 * Determines whether two nonzero numbers are with 5% of one another.
 *
 * @param {number} x
 * @param {number} y
 * @return {boolean}
 */
pagespeed.MobUtil.aboutEqual = function(x, y) {
  var ratio = (x > y) ? (y / x) : (x / y);
  return (ratio > 0.95);
};


/**
 * Adds new styles to an element.
 *
 * @param {!Element} element
 * @param {string} newStyles
 */
pagespeed.MobUtil.addStyles = function(element, newStyles) {
  if (newStyles && (newStyles.length != 0)) {
    var style = element.getAttribute('style') || '';
    if ((style.length > 0) && (style[style.length - 1] != ';')) {
      style += ';';
    }
    style += newStyles;
    element.setAttribute('style', style);
  }
};


/**
 * Gets the bounding box of a node, taking into account any global
 * scroll position.
 * @param {!Element} node
 * @return {ClientRect}
 */
pagespeed.MobUtil.boundingRect = function(node) {
  var rect = node.getBoundingClientRect();

  // getBoundingClientRect() is w.r.t. the viewport. Add the amount scrolled to
  // calculate the absolute position of the element.
  // From https://developer.mozilla.org/en-US/docs/DOM/window.scrollX
  var body = document.body;
  var scrollElement = document.documentElement || body.parentNode || body;
  var scrollX = 'pageXOffset' in window ? window.pageXOffset :
      scrollElement.scrollLeft;
  var scrollY = 'pageYOffset' in window ? window.pageYOffset :
      scrollElement.scrollTop;

  rect.top += scrollY;
  rect.bottom += scrollY;
  rect.left += scrollX;
  rect.right += scrollX;
  return rect;
};


/**
 * @param {Node} img
 * @return {boolean}
 */
pagespeed.MobUtil.isSinglePixel = function(img) {
  return img.naturalHeight == 1 && img.naturalWidth == 1;
};


/**
 * Returns the background image for an element as a URL string.
 * @param {!Element} element
 * @return {?string}
 */
pagespeed.MobUtil.findBackgroundImage = function(element) {
  var image = null;
  if ((element.tagName != 'SCRIPT') && (element.tagName != 'STYLE') &&
      element.style) {
    var computedStyle = window.getComputedStyle(element);
    if (computedStyle) {
      image = computedStyle.getPropertyValue('background-image');
      if (image == 'none') {
        image = null;
      }
      if (image) {
        // remove 'url(' prefix and ')' suffix.
        if ((image.length > 5) &&
            (image.indexOf('url(') == 0) &&
            (image[image.length - 1] == ')')) {
          image = image.substring(4, image.length - 1);
        }
      }
    }
  }
  return image;
};


/**
 * Detect if in iframe. Borrowed from
 * http://stackoverflow.com/questions/326069/how-to-identify-if-a-webpage-is-being-loaded-inside-an-iframe-or-directly-into-t
 * @return {boolean}
 */
pagespeed.MobUtil.inFriendlyIframe = function() {
  if ((window.parent != null) && (window != window.parent)) {
    try {
      if (window.parent.document.domain == document.domain) {
        return true;
      }
    } catch (err) {
    }
  }
  return false;
};


/**
 * @return {boolean}
 */
pagespeed.MobUtil.possiblyInQuirksMode = function() {
  // http://stackoverflow.com/questions/627097/how-to-tell-if-a-browser-is-in-quirks-mode
  return document.compatMode !== 'CSS1Compat';
};


/**
 * TODO(jmarantz): Think of faster algorithm.
 * @param {!Array.<!ClientRect>} rects
 * @return {boolean}
 */
pagespeed.MobUtil.hasIntersectingRects = function(rects) {
  // N^2 loop to determine whether there are any intersections.
  for (var i = 0; i < rects.length; ++i) {
    for (var j = i + 1; j < rects.length; ++j) {
      if (pagespeed.MobUtil.intersects(rects[i], rects[j])) {
        return true;
      }
    }
  }
  return false;
};


/**
 * Creates an XPath for a node.
 * http://stackoverflow.com/questions/2661818/javascript-get-xpath-of-a-node
 * @param {Node} node
 * @return {?string}
 */
pagespeed.MobUtil.createXPathFromNode = function(node) {
  var allNodes = document.getElementsByTagName('*');
  var i, segs = [], sib;
  for (; goog.dom.isElement(node); node = node.parentNode) {
    if (node.hasAttribute('id')) {
      var uniqueIdCount = 0;
      for (var n = 0; (n < allNodes.length) && (uniqueIdCount <= 1); ++n) {
        if (allNodes[n].hasAttribute('id') && allNodes[n].id == node.id) {
          ++uniqueIdCount;
        }
      }
      if (uniqueIdCount == 1) {
        segs.unshift('id("' + node.getAttribute('id') + '")');
        return segs.join('/');
      } else {
        segs.unshift(node.localName.toLowerCase() + '[@id="' +
                     node.getAttribute('id') + '"]');
      }
    } else if (node.hasAttribute('class')) {
      segs.unshift(node.localName.toLowerCase() + '[@class="' +
                   node.getAttribute('class') + '"]');
    } else {
      for (i = 1, sib = node.previousSibling; sib;
           sib = sib.previousSibling) {
        if (sib.localName == node.localName) {
          i++;
        }
      }
      segs.unshift(node.localName.toLowerCase() + '[' + i + ']');
    }
  }
  return segs.length ? '/' + segs.join('/') : null;
};


/**
 * Returns a counts of the number of nodes in a DOM subtree.
 * @param {Node} node
 * @return {number}
 */
pagespeed.MobUtil.countNodes = function(node) {
  // TODO(jmarantz): microbenchmark this recursive implementation against
  // document.querySelectorAll('*').length
  var count = 1;
  for (var child = node.firstChild; child; child = child.nextSibling) {
    count += pagespeed.MobUtil.countNodes(child);
  }
  return count;
};
