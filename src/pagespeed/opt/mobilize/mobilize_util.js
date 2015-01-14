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

goog.require('goog.color');
goog.require('goog.dom');
goog.require('goog.math.Box');
goog.require('goog.string');



/**
 * @fileoverview
 * Stateless utility functions for PageSped mobilization.
 */


/**
 * Ascii code for '0'
 * @private {number}
 */
pagespeed.MobUtil.ASCII_0_ = '0'.charCodeAt(0);


/**
 * Ascii code for '9'
 * @private {number}
 */
pagespeed.MobUtil.ASCII_9_ = '9'.charCodeAt(0);



/**
 * Create a rectangle (Rect) struct.
 * @struct
 * @constructor
 */
pagespeed.MobUtil.Rect = function() {
  /** @type {number} Top of the bounding box */
  this.top = 0;
  /** @type {number} Left of the bounding box */
  this.left = 0;
  /** @type {number} Right of the bounding box */
  this.right = 0;
  /** @type {number} Bottom of the bounding box */
  this.bottom = 0;
  /** @type {number} Width of the content (e.g. image). This may be different
   * from width of the bounding box. */
  this.width = 0;
  /** @type {number} Height of the content (e.g., image). This may be different
   * from height of the bounding box. */
  this.height = 0;
};



/**
 * Create a dimensions (Dimensions) struct.
 * @struct
 * @param {number} width
 * @param {number} height
 * @constructor
 */
pagespeed.MobUtil.Dimensions = function(width, height) {
  /** @type {number} width */
  this.width = width;
  /** @type {number} height */
  this.height = height;
};


/**
 * Returns whether the character at index's ascii code is a digit.
 * @param {string} str
 * @param {number} index
 * @return {boolean}
 */
pagespeed.MobUtil.isDigit = function(str, index) {
  if (str.length <= index) {
    return false;
  }
  var ascii = str.charCodeAt(index);
  return ((ascii >= pagespeed.MobUtil.ASCII_0_) &&
          (ascii <= pagespeed.MobUtil.ASCII_9_));
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
 * @param {number|?string} value Attribute value.
 * @return {?number} integer value in pixels or null.
 */
pagespeed.MobUtil.pixelValue = function(value) {
  var ret = null;
  if (value && (typeof value == 'string')) {
    var px = value.indexOf('px');
    if (px != -1) {
      value = value.substring(0, px);
    }
    if (pagespeed.MobUtil.isDigit(value, value.length - 1)) {
      ret = parseInt(value, 10);
      if (isNaN(ret)) {
        ret = null;
      }
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
 * @return {!goog.math.Box}
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

  return new goog.math.Box(rect.top + scrollY,
                           rect.right + scrollX,
                           rect.bottom + scrollY,
                           rect.left + scrollX);
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

          // TODO(jmarantz): change logic to handle multiple comma-separated
          // background images, which will be overlayed on one another.  E.g.
          //     backgrond-image: url(a.png), url(b.png);
          //  if ((image.indexOf(',') != -1) ||
          //    (image.indexOf('(') != -1) ||
          //    (image.indexOf('}') != -1)) {
          //   debugger;
          // }
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
 * @param {!Array.<!goog.math.Box>} rects
 * @return {boolean}
 */
pagespeed.MobUtil.hasIntersectingRects = function(rects) {
  // N^2 loop to determine whether there are any intersections.
  for (var i = 0; i < rects.length; ++i) {
    for (var j = i + 1; j < rects.length; ++j) {
      if (goog.math.Box.intersects(rects[i], rects[j])) {
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


/**
 * If a node is actually an element, returns it as an element.  Otherwise,
 * returns null.
 *
 * @param {!Node} node
 * @return {?Element} element
 */
pagespeed.MobUtil.castElement = function(node) {
  if (goog.dom.isElement(node)) {
    return /** @type {Element} */ (node);
  }
  return null;
};


/**
 * Enum for image source. We consider images from 'IMG' tag, 'SVG' tag, and
 * background.
 * @enum {string}
 * @export
 */
pagespeed.MobUtil.ImageSource = {
  // TODO(huibao): Consider <INPUT> tag, which can also have image src.
  IMG: 'IMG',
  SVG: 'SVG',
  BACKGROUND: 'background-image'
};



/**
 * Theme data.
 * @param {!goog.color.Rgb} frontColor
 * @param {!goog.color.Rgb} backColor
 * @param {!Element} menuButton
 * @param {!Element} logoSpan
 * @constructor
 */
pagespeed.MobUtil.ThemeData = function(frontColor, backColor, menuButton,
                                       logoSpan) {

  /** @type {!goog.color.Rgb} */
  this.menuFrontColor = frontColor;
  /** @type {!goog.color.Rgb} */
  this.menuBackColor = backColor;
  /** @type {!Element} */
  this.menuButton = menuButton;
  /** @type {!Element} */
  this.logoSpan = logoSpan;
};


/**
 * Return text between the leftmost '(' and the rightmost ')'. If there is not
 * a pair of '(', ')', return null.
 * @param {string} str
 * @return {?string}
 */
pagespeed.MobUtil.textBetweenBrackets = function(str) {
  var left = str.indexOf('(');
  var right = str.lastIndexOf(')');
  if (left >= 0 && right > left) {
    return str.substring(left + 1, right);
  }
  return null;
};


/**
 * Convert color string '(n1, n2, ..., nm)' to numberical array
 * [n1, n2, ..., nm]. If the color string does not have a valid format, return
 * null. Note that this only understandards the color formats that can occur
 * in getComputedStyle output, not all formats that can be used by CSS.
 * See: http://dev.w3.org/csswg/cssom/#serialize-a-css-component-value
 * @param {string} str
 * @return {Array.<number>}
 */
pagespeed.MobUtil.colorStringToNumbers = function(str) {
  var subStr = pagespeed.MobUtil.textBetweenBrackets(str);
  if (!subStr) {
    // Mozilla likes to return 'transparent' for things with alpha == 0,
    // so handle that here.
    if (str == 'transparent') {
      return [0, 0, 0, 0];
    }
    return null;
  }
  // TODO(huibao): Verify that rgb( and rgba( are present and used
  // appropriately.
  subStr = subStr.split(',');
  var numbers = [];
  for (var i = 0, len = subStr.length; i < len; ++i) {
    if (i != 3) {
      // Non-alpha channels are integers.
      numbers[i] = parseInt(subStr[i], 10);
    } else {
      // Alpha is floating point.
      numbers[i] = parseFloat(subStr[i]);
    }
    if (isNaN(numbers[i])) {
      return null;
    }
  }

  // Only 3 or 4 compontents is normal.
  if (numbers.length == 3 || numbers.length == 4) {
    return numbers;
  } else {
    return null;
  }
};


/**
 * Convert a color array to a string. For example, [0, 255, 255] will be
 * converted to '#00FFFF'.
 * @param {!Array.<number>} color
 * @return {string}
 */
pagespeed.MobUtil.colorNumbersToString = function(color) {
  for (var i = 0, len = color.length; i < len; ++i) {
    var val = Math.round(color[i]);
    if (val < 0) {
      val = 0;
    } else if (val > 255) {
      val = 255;
    }
    color[i] = val;
  }
  return goog.color.rgbArrayToHex(color);
};


/**
 * Extract characters 'a'-'z', 'A'-'Z', and '0'-'9' from string 'str'.
 * 'A'-'Z' are converted to lower case.
 * @param {string} str
 * @return {string}
 */
pagespeed.MobUtil.stripNonAlphaNumeric = function(str) {
  str = str.toLowerCase();
  var newString = '';
  for (var i = 0, len = str.length; i < len; ++i) {
    var ch = str.charAt(i);
    if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
      newString += ch;
    }
  }
  return newString;
};


/**
 * Extracts the characters 'a'-'z', 'A'-'Z', and '0'-'9' from both strings
 * and returns whether the first converted string contains the latter.
 * If it does, return 1; otherwise, return 0.
 * @param {string} str
 * @param {string} pattern
 * @return {number}
 */
pagespeed.MobUtil.findPattern = function(str, pattern) {
  return (pagespeed.MobUtil.stripNonAlphaNumeric(str).indexOf(
      pagespeed.MobUtil.stripNonAlphaNumeric(pattern)) >= 0 ? 1 : 0);
};


/**
 * Remove the substring after 'symbol' multiple times from 'str'.
 * @param {string} str
 * @param {string} symbol
 * @param {number} num
 * @return {string}
 */
pagespeed.MobUtil.removeSuffixNTimes = function(str, symbol, num) {
  var len = str.length;
  for (var i = 0; i < num; ++i) {
    var pos = str.lastIndexOf(symbol, len - 1);
    if (pos >= 0) {
      len = pos;
    } else {
      break;
    }
  }
  if (pos >= 0) {
    return str.substring(0, len);
  }
  return str;
};


/**
 * Return the origanization name.
 * For example, if the domain is 'sub1.sub2.ORGANIZATION.com.uk.psproxy.com'
 * this method will return 'organization'.
 * @return {?string}
 */
pagespeed.MobUtil.getSiteOrganization = function() {
  // TODO(huibao): Compute the organization name in C++ and export it to
  // JS code as a variable. This can be done by using
  // DomainLawyer::StripProxySuffix to strip any proxy suffix and then using
  // DomainRegistry::MinimalPrivateSuffix.
  var org = document.domain;
  var segments = org.toLowerCase().split('.');
  var len = segments.length;
  if (len > 4 && segments[len - 3].length == 2) {
    // Contry level domain has exactly 2 characters. It will be skipped if
    // the origin has it.
    // http://en.wikipedia.org/wiki/List_of_Internet_top-level_domains
    return segments[len - 5];
  } else if (len > 3) {
    return segments[len - 4];
  } else {
    return null;
  }
};


/**
 * Returns file name of the resource. File name is defined as the string between
 * the last '/' and the last '.'. For example, for
 * 'http://www.example.com/FILE_NAME.jpg', it will return 'FILE_NAME'.
 * @param {?string} url
 * @return {string}
 */
pagespeed.MobUtil.resourceFileName = function(url) {
  if (!url || url.indexOf('data:image/') >= 0) {
    return '';
  }

  var lastSlash = url.lastIndexOf('/');
  if (lastSlash < 0) {
    lastSlash = 0;
  } else {
    ++lastSlash;  // Skip the slash
  }
  var lastDot = url.indexOf('.', lastSlash);
  if (lastDot < 0) {
    lastDot = url.length;
  }

  return url.substring(lastSlash, lastDot);
};


/**
 * Try to add proxy suffix and 'www.' prefix to URL so it has the same origin
 * as the HTML.
 *
 * For example, if the origin is 'sub.example.com.psproxy.net',
 * URL 'http://sub.example.com/image.jpg' will be converted to
 * 'http://sub.example.com.psproxy.net/image.jpg'.
 *
 * As another example, if the origin is 'www.example.com.psproxy.net',
 * URL 'http://example.com/image.jpg' and
 * URL 'http://example.com.psproxy.net/image.jpg' will be converted to
 * 'http://www.example.com.psproxy.net/image.jpg'.
 *
 * @param {string} url
 * @return {string}
 */
pagespeed.MobUtil.proxyImageUrl = function(url) {
  if (!url) {
    return '';
  }

  // If this URL has the same origin as the HTML, we don't need to do anything.
  if (!pagespeed.MobUtil.isCrossOrigin(url)) {
    return url;
  }

  // Skip suffix after last 2 dots. For example, 'www.example.com.psproxy.net'
  // will become 'www.example.com'.
  var domain = document.domain;
  var domainWithoutProxy =
      pagespeed.MobUtil.removeSuffixNTimes(domain, '.', 2);

  // If domain is 'www.example.com' while image is
  // 'http://example.com/image.url', modify the image URL so it has the same
  // domain as the HTML.
  // TODO(huibao): Investigate whether to add sub-domain prefix (including
  // 'www.') if this is missing in the URL.
  var pos = null;
  if (domainWithoutProxy.indexOf('www.') == 0 && url.indexOf('//www.') < 0) {
    var domainWithoutWwwProxy = domainWithoutProxy.substring(4);
    pos = url.indexOf(domainWithoutWwwProxy);
    if (pos >= 0) {
      url = url.substring(0, pos) + domainWithoutProxy +
          url.substring(pos + domainWithoutWwwProxy.length);
    }
  }

  if (!pagespeed.MobUtil.isCrossOrigin(url)) {
    return url;
  }

  pos = url.indexOf(domainWithoutProxy);
  if (pos >= 0) {
    url = url.substring(0, pos) + domain +
        url.substring(pos + domainWithoutProxy.length, url.length);
  }

  return url;
};


/**
 * Extract the image from IMG and SVG tags, or from the background.
 * @param {!Element} element
 * @param {!pagespeed.MobUtil.ImageSource} source
 * @return {?string}
 */
pagespeed.MobUtil.extractImage = function(element, source) {
  var imageSrc = null;
  switch (source) {
    case pagespeed.MobUtil.ImageSource.IMG:
      if (element.tagName == source) {
        imageSrc = element.src;
      }
      break;

    case pagespeed.MobUtil.ImageSource.SVG:
      if (element.tagName == source) {
        var svgString = new XMLSerializer().serializeToString(element);
        var domUrl = self.URL || self.webkitURL || self;
        var svgBlob = new Blob([svgString],
                               {type: 'image/svg+xml;charset=utf-8'});
        imageSrc = domUrl.createObjectURL(svgBlob);
      }
      break;

    case pagespeed.MobUtil.ImageSource.BACKGROUND:
      imageSrc = pagespeed.MobUtil.findBackgroundImage(element);
      break;
  }
  if (imageSrc) {
    return pagespeed.MobUtil.proxyImageUrl(imageSrc);
  }
  return null;
};


/**
 * Return whether the resource URL has an origin different from HTML.
 * @param {string} url
 * @return {boolean}
 */
pagespeed.MobUtil.isCrossOrigin = function(url) {
  return (!goog.string.startsWith(url, document.location.origin + '/') &&
          !goog.string.startsWith(url, 'data:image/'));
};


/**
 * Return bounding box and size of the element.
 * @param {!Element} element
 * @return {!pagespeed.MobUtil.Rect}
 */
pagespeed.MobUtil.boundingRectAndSize = function(element) {
  var rect = pagespeed.MobUtil.boundingRect(element);
  var psRect = new pagespeed.MobUtil.Rect();
  psRect.top = rect.top;
  psRect.bottom = rect.bottom;
  psRect.left = rect.left;
  psRect.right = rect.right;
  psRect.height = rect.bottom - rect.top;
  psRect.width = rect.right - rect.left;
  return psRect;
};


/**
 * Computes whether the element is positioned off the screen.
 * @param {!CSSStyleDeclaration} style
 * @return {boolean}
 */
pagespeed.MobUtil.isOffScreen = function(style) {
  var top = pagespeed.MobUtil.pixelValue(style.top);
  var left = pagespeed.MobUtil.pixelValue(style.left);
  return (((top != null) && (top < -100)) ||
          ((left != null) && (left < -100)));
};


/**
 * Take a JS string and escape it to obtain a CSS string1 (double quoted).
 * See http://www.w3.org/TR/css3-selectors/#w3cselgrammar
 * @param {string} unescaped JS string
 * @return {string} escaped, quoted CSS string1
 */
pagespeed.MobUtil.toCssString1 = function(unescaped) {
  // There are actually relatively few forbidden characters [^\r\n\f\\"], so
  // just replace each one of them by a safe escape.
  // All the escapes start with backslash, so escape existing backslashes first.
  var result = unescaped.replace(/\\/g, '\\\\');
  result = result.replace(/"/g, '\\"');
  result = result.replace(/\n/g, '\\a ');
  result = result.replace(/\f/g, '\\c ');
  result = result.replace(/\r/g, '\\d ');
  return '"' + result + '"';
};
