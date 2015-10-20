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

goog.provide('mob.util');
goog.provide('mob.util.BeaconEvents');
goog.provide('mob.util.Dimensions');
goog.provide('mob.util.ElementClass');
goog.provide('mob.util.ElementId');
goog.provide('mob.util.ImageSource');
goog.provide('mob.util.Rect');
goog.provide('mob.util.ThemeData');

goog.require('goog.asserts');
goog.require('goog.color');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.labs.userAgent.browser');
goog.require('goog.math.Box');
goog.require('goog.string');
goog.require('goog.uri.utils');


/**
 * @fileoverview
 * Stateless utility functions for PageSped mobilization.
 */


/**
 * @private {!Window}
 */
mob.util.window_ =
    (typeof extension != 'undefined' && extension.hasOwnProperty('target')) ?
        extension.target :
        window;


/**
 * The window object to use, to make functions work both in a normal browser and
 * in WKH.
 * @return {!Window}
 */
mob.util.getWindow = function() {
  return mob.util.window_;
};


/**
 * The ids of elements used for mobilization.
 * @enum {string}
 */
mob.util.ElementId = {
  CLICK_DETECTOR_DIV: 'psmob-click-detector-div',
  CONFIG_IFRAME: 'ps-hidden-iframe',
  DIALER_BUTTON: 'psmob-dialer-button',
  HEADER_BAR: 'psmob-header-bar',
  IFRAME: 'psmob-iframe',
  IFRAME_CONTAINER: 'psmob-iframe-container',
  LOGO_IMAGE: 'psmob-logo-image',
  LOGO_SPAN: 'psmob-logo-span',
  MAP_BUTTON: 'psmob-map-button',
  MENU_BUTTON: 'psmob-menu-button',
  NAV_PANEL: 'psmob-nav-panel',
  PROGRESS_LOG: 'ps-progress-log',
  PROGRESS_REMOVE: 'ps-progress-remove',
  PROGRESS_SCRIM: 'ps-progress-scrim',
  PROGRESS_SHOW_LOG: 'ps-progress-show-log',
  PROGRESS_SPAN: 'ps-progress-span',
  SPACER: 'psmob-spacer'
};


/**
 * Class names used for mobilization.
 * @enum {string}
 */
mob.util.ElementClass = {
  BUTTON: 'psmob-button',
  BUTTON_ICON: 'psmob-button-icon',
  BUTTON_TEXT: 'psmob-button-text',
  HIDE: 'psmob-hide',
  IOS_WEBVIEW: 'ios-webview',
  LABELED: 'psmob-labeled',
  LOGO_CHOOSER_CHOICE: 'psmob-logo-chooser-choice',
  LOGO_CHOOSER_COLOR: 'psmob-logo-chooser-color',
  LOGO_CHOOSER_COLUMN_HEADER: 'psmob-logo-chooser-column-header',
  LOGO_CHOOSER_CONFIG_FRAGMENT: 'psmob-logo-chooser-config-fragment',
  LOGO_CHOOSER_IMAGE: 'psmob-logo-chooser-image',
  LOGO_CHOOSER_SWAP: 'psmob-logo-chooser-swap',
  LOGO_CHOOSER_TABLE: 'psmob-logo-chooser-table',
  MENU_EXPAND_ICON: 'psmob-menu-expand-icon',
  NOSCROLL: 'psmob-noscroll',
  OPEN: 'psmob-open',
  SHOW_BUTTON_TEXT: 'psmob-show-button-text',
  SINGLE_COLUMN: 'psmob-single-column',
  THEME_CONFIG: 'psmob-theme-config'
};


/**
 * Ascii code for '0'
 * @private {number}
 */
mob.util.ASCII_0_ = '0'.charCodeAt(0);


/**
 * Ascii code for '9'
 * @private {number}
 */
mob.util.ASCII_9_ = '9'.charCodeAt(0);



/**
 * Create a rectangle (Rect) struct.
 * @struct
 * @constructor
 */
mob.util.Rect = function() {
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
mob.util.Dimensions = function(width, height) {
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
mob.util.isDigit = function(str, index) {
  if (str.length <= index) {
    return false;
  }
  var ascii = str.charCodeAt(index);
  return ((ascii >= mob.util.ASCII_0_) && (ascii <= mob.util.ASCII_9_));
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
mob.util.pixelValue = function(value) {
  var ret = null;
  if (value && (typeof value == 'string')) {
    var px = value.indexOf('px');
    if (px != -1) {
      value = value.substring(0, px);
    }
    if (mob.util.isDigit(value, value.length - 1)) {
      ret = parseInt(value, 10);
      if (isNaN(ret)) {
        ret = null;
      }
    }
  }
  return ret;
};


/**
 * Adds new styles to an element.
 *
 * @param {!Element} element
 * @param {string} newStyles
 */
mob.util.addStyles = function(element, newStyles) {
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
mob.util.boundingRect = function(node) {
  var rect = node.getBoundingClientRect();

  var win = mob.util.getWindow();
  // getBoundingClientRect() is w.r.t. the viewport. Add the amount scrolled to
  // calculate the absolute position of the element.
  // From https://developer.mozilla.org/en-US/docs/DOM/window.scrollX
  var body = win.document.body;
  var scrollElement = win.document.documentElement || body.parentNode || body;
  var scrollX =
      'pageXOffset' in win ? win.pageXOffset : scrollElement.scrollLeft;
  var scrollY =
      'pageYOffset' in win ? win.pageYOffset : scrollElement.scrollTop;

  return new goog.math.Box(rect.top + scrollY,
                           rect.right + scrollX,
                           rect.bottom + scrollY,
                           rect.left + scrollX);
};


/**
 * Returns the background image for an element as a URL string.
 * @param {!Element} element
 * @return {?string}
 */
mob.util.findBackgroundImage = function(element) {
  var image = null;
  var nodeName = element.nodeName.toUpperCase();
  if ((nodeName != goog.dom.TagName.SCRIPT) &&
      (nodeName != goog.dom.TagName.STYLE) && element.style) {
    var computedStyle = mob.util.getWindow().getComputedStyle(element);
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
          return image;

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
  return null;
};


/**
 * Detect if in iframe. Borrowed from
 * http://stackoverflow.com/questions/326069/how-to-identify-if-a-webpage-is-being-loaded-inside-an-iframe-or-directly-into-t
 * @return {boolean}
 */
mob.util.inFriendlyIframe = function() {
  var win = mob.util.getWindow();
  if ((win.parent != null) && (win != win.parent)) {
    try {
      if (win.parent.document.domain == win.document.domain) {
        return true;
      }
    } catch (err) {
    }
  }
  return false;
};


/**
 * Returns a counts of the number of nodes in a DOM subtree.
 * @param {?Node} node
 * @return {number}
 */
mob.util.countNodes = function(node) {
  if (!node) {
    return 0;
  }
  // TODO(jmarantz): microbenchmark this recursive implementation against
  // window.document.querySelectorAll('*').length
  var count = 1;
  for (var child = node.firstChild; child; child = child.nextSibling) {
    count += mob.util.countNodes(child);
  }
  return count;
};


/**
 * Enum for image source. We consider images from IMG tag, SVG tag, and
 * background.
 * @enum {string}
 */
mob.util.ImageSource = {
  // TODO(huibao): Consider <INPUT> tag, which can also have image src.
  IMG: 'IMG',
  SVG: 'SVG',
  BACKGROUND: 'background-image'
};



/**
 * Theme data.
 * @param {?string} logoUrl Url for the logo.
 * @param {!goog.color.Rgb} frontColor
 * @param {!goog.color.Rgb} backColor
 * @constructor
 * @struct
 */
mob.util.ThemeData = function(logoUrl, frontColor, backColor) {
  /** @type {?string} */
  this.logoUrl = logoUrl;
  /** @type {!goog.color.Rgb} */
  this.menuFrontColor = frontColor;
  /** @type {!goog.color.Rgb} */
  this.menuBackColor = backColor;
};


/**
 * Return text between the leftmost '(' and the rightmost ')'. If there is not
 * a pair of '(', ')', return null.
 * @param {string} str
 * @return {?string}
 */
mob.util.textBetweenBrackets = function(str) {
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
 * @return {?Array.<number>}
 */
mob.util.colorStringToNumbers = function(str) {
  var subStr = mob.util.textBetweenBrackets(str);
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
 * converted to '#00ffff'.
 * @param {!goog.color.Rgb} color
 * @return {string}
 */
mob.util.colorNumbersToString = function(color) {
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
mob.util.stripNonAlphaNumeric = function(str) {
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
mob.util.findPattern = function(str, pattern) {
  str = mob.util.stripNonAlphaNumeric(str);
  pattern = mob.util.stripNonAlphaNumeric(pattern);
  return (str.indexOf(pattern) >= 0 ? 1 : 0);
};


/**
 * Return the origanization name.
 * For example, if the domain is 'sub1.sub2.ORGANIZATION.com.uk.psproxy.com'
 * this method will return 'organization'.
 * @return {?string}
 */
mob.util.getSiteOrganization = function() {
  // TODO(huibao): Compute the organization name in C++ and export it to
  // JS code as a variable. This can be done by using
  // DomainLawyer::StripProxySuffix to strip any proxy suffix and then using
  // DomainRegistry::MinimalPrivateSuffix.
  var org = mob.util.getWindow().document.domain;
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
mob.util.resourceFileName = function(url) {
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
 * Add proxy suffix and/or 'www.' prefix to URL, if it makes the URL have the
 * same orgin as the HTML; otherwise, return the original URL. This method also
 * replaces the scheme with that of the HTML when it modifies the URL.
 *
 * For example, if the origin is 'http://sub.example.com.psproxy.net',
 * URL 'http://sub.example.com/image.jpg' will be converted to
 * 'http://sub.example.com.psproxy.net/image.jpg'.
 *
 * As another example, if the origin is 'https://www.example.com.psproxy.net',
 * URL 'http://example.com/image.jpg' and
 * URL 'http://example.com.psproxy.net/image.jpg' will be converted to
 * 'https://www.example.com.psproxy.net/image.jpg'.
 *
 * TODO(jud): Remove this function after verifying we no longer have a need for
 * it. It is currently unused.
 *
 * @param {string} url
 * @param {?string=} opt_origin Origin used for testing
 * @return {string}
 */
mob.util.proxyImageUrl = function(url, opt_origin) {
  // Note that we originally used document.location.origin here but it returns
  // null on the galaxy s2 stock browser.
  var origin = goog.uri.utils.getHost(
      opt_origin || mob.util.getWindow().document.location.href);
  var originDomain = goog.uri.utils.getDomain(origin);
  var urlDomain = goog.uri.utils.getDomain(url);

  if (originDomain == null || urlDomain == null) {
    return url;
  }

  var canProxy = false;
  if (originDomain == urlDomain) {
    return url;
  } else if (originDomain.indexOf(urlDomain) >= 0) {
    var originDomainPieces = originDomain.split('.');
    var len = originDomainPieces.length;
    if (len >= 3 &&
        (urlDomain == originDomainPieces.slice(0, len - 2).join('.') ||
         (originDomainPieces[0] == 'www' &&
          (urlDomain == originDomainPieces.slice(1, len - 2).join('.') ||
           urlDomain == originDomainPieces.slice(1, len).join('.'))))) {
      canProxy = true;
    }
  }

  if (canProxy) {
    var urlPos = url.indexOf(urlDomain) + urlDomain.length;
    return (origin + url.substring(urlPos));
  }
  return url;
};


/**
 * Extract the image from IMG and SVG tags, or from the background.
 * @param {!Element} element
 * @param {!mob.util.ImageSource} source
 * @return {?string}
 */
mob.util.extractImage = function(element, source) {
  var imageSrc = null;
  switch (source) {
    case mob.util.ImageSource.IMG:
      if (element.nodeName == source) {
        imageSrc = element.src;
      }
      break;

    case mob.util.ImageSource.SVG:
      if (element.nodeName == source) {
        var svgString = new XMLSerializer().serializeToString(element);
        var domUrl = self.URL || self.webkitURL || self;
        var svgBlob = new Blob([svgString],
                               {type: 'image/svg+xml;charset=utf-8'});
        imageSrc = domUrl.createObjectURL(svgBlob);
      }
      break;

    case mob.util.ImageSource.BACKGROUND:
      imageSrc = mob.util.findBackgroundImage(element);
      break;
  }
  if (imageSrc) {
    return imageSrc;
  }
  return null;
};


/**
 * Synthesize an image using the specified color.
 * @param {string} imageBase64
 * @param {!goog.color.Rgb} color
 * @return {string}
 */
mob.util.synthesizeImage = function(imageBase64, color) {
  goog.asserts.assert(imageBase64.length > 16);
  var imageTemplate = mob.util.getWindow().atob(imageBase64);
  var imageData = imageTemplate.substring(0, 13) +
                  String.fromCharCode(color[0], color[1], color[2]) +
                  imageTemplate.substring(16, imageTemplate.length);
  var imageUrl =
      'data:image/gif;base64,' + mob.util.getWindow().btoa(imageData);
  return imageUrl;
};


/**
 * Return whether the resource URL has an origin different from HTML.
 * @param {string} url
 * @return {boolean}
 */
mob.util.isCrossOrigin = function(url) {
  var origin = mob.util.getWindow().document.location.origin + '/';
  return (!goog.string.startsWith(url, origin) &&
          !goog.string.startsWith(url, 'data:image/'));
};


/**
 * Return bounding box and size of the element.
 * @param {!Element} element
 * @return {!mob.util.Rect}
 */
mob.util.boundingRectAndSize = function(element) {
  var rect = mob.util.boundingRect(element);
  var psRect = new mob.util.Rect();
  psRect.top = rect.top;
  psRect.bottom = rect.bottom;
  psRect.left = rect.left;
  psRect.right = rect.right;
  psRect.height = rect.bottom - rect.top;
  psRect.width = rect.right - rect.left;
  return psRect;
};


/**
 * Logs a message to the console, only in debug mode, and if that functionality
 * has not been disabled by the site.
 * @param {string} message
 */
mob.util.consoleLog = function(message) {
  // TODO(jud): Consider using goog.log.
  if (mob.util.getWindow().psDebugMode && console && console.log) {
    console.log(message);
  }
};


/**
 * Constants used for beacon events.
 * @enum {string}
 */
mob.util.BeaconEvents = {
  CALL_CONVERSION_RESPONSE: 'call-conversion-response',
  CALL_FALLBACK_NUMBER: 'call-fallback-number',
  CALL_GV_NUMBER: 'call-gv-number',
  INITIAL_EVENT: 'initial-event',
  LOAD_EVENT: 'load-event',
  MAP_BUTTON: 'psmob-map-button',
  MENU_BUTTON_CLOSE: 'psmob-menu-button-close',
  MENU_BUTTON_OPEN: 'psmob-menu-button-open',
  SUBMENU_CLOSE: 'psmob-submenu-close',
  SUBMENU_OPEN: 'psmob-submenu-open',
  MENU_NAV_CLICK: 'psmob-menu-nav-click',
  NAV_DONE: 'nav-done',
  PHONE_BUTTON: 'psmob-phone-dialer'
};


/**
 * Track a mobilization event by sending to the beacon handler specified via
 * RewriteOption MobBeaconUrl. This will wait at most 500ms before calling
 * opt_callback to balance giving the browser enough time to send the event, but
 * not blocking the event on slow network requests.
 * @param {mob.util.BeaconEvents} beaconEvent Identifier for the event
 *     being tracked.
 * @param {?Function=} opt_callback Optional callback to be run when the 204
 *     finishes loading, or 500ms have elapsed, whichever happens first.
 * @param {string=} opt_additionalParams An additional string to be added to the
 * end of the request.
 */
mob.util.sendBeaconEvent = function(beaconEvent, opt_callback,
                                    opt_additionalParams) {
  var win = mob.util.getWindow();
  if (!win.psMobBeaconUrl) {
    if (opt_callback) {
      opt_callback();
      return;
    }
  }

  var pingUrl = win.psMobBeaconUrl + '?id=psmob' +
                '&url=' + encodeURIComponent(win.document.URL) + '&el=' +
                beaconEvent;
  if (win.psMobBeaconCategory) {
    pingUrl += '&category=' + win.psMobBeaconCategory;
  }
  if (opt_additionalParams) {
    pingUrl += opt_additionalParams;
  }
  var img = win.document.createElement(goog.dom.TagName.IMG);
  img.src = pingUrl;

  // Ensure that the callback is only called once, even though it can be called
  // from either the beacon being loaded or from a 500ms timeout.
  if (opt_callback) {
    var callbackRunner = mob.util.runCallbackOnce_(opt_callback);
    img.addEventListener(goog.events.EventType.LOAD, callbackRunner);
    img.addEventListener(goog.events.EventType.ERROR, callbackRunner);
    win.setTimeout(callbackRunner, 500);
  }
};


/**
 * Ensure that the provided callback only gets run once.
 * @param {!Function} callback
 * @return {!function()}
 * @private
 */
mob.util.runCallbackOnce_ = function(callback) {
  var callbackCalled = false;
  return function() {
    if (!callbackCalled) {
      callbackCalled = true;
      callback();
    }
  };
};


/**
 * Get the zoom level, used for scaling the header bar and nav panel.
 * @return {number}
 */
mob.util.getZoomLevel = function() {
  var win = mob.util.getWindow();
  var zoom = 1;
  if (win.psDeviceType != 'desktop') {
    // screen.width does not update on rotation on ios, but it does on android,
    // so compensate for that here.
    if ((Math.abs(win.orientation) == 90) && (screen.height > screen.width)) {
      zoom *= (win.innerHeight / screen.width);
    } else {
      zoom *= win.innerWidth / screen.width;
    }
  }
  // Android browser does not seem to take the pixel ratio into account in the
  // values it returns for screen.width and screen.height.
  if (goog.labs.userAgent.browser.isAndroidBrowser()) {
    zoom *= goog.dom.getPixelRatio();
  }
  return zoom;
};
