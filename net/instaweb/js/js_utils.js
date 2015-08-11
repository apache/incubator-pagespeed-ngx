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
 * @fileoverview Common set of JS snippets and utility functions for use in
 * static JS files. To use, include goog.require('pagespeedutils') in the JS
 * file and compile with the psa_js_library build rule, which automatically
 * includes this file as a dependency.
 */

goog.provide('pagespeedutils');


/**
 * Define the maximum size (in bytes) of a POST that the server will accept. We
 * shouldn't send more data than this. This must match kMaxPostSizeBytes.
 */
pagespeedutils.MAX_POST_SIZE = 131072;


/**
 * Send the beacon as an AJAX POST request to the server.
 * @param {string} beaconUrl The URL the beacon should be sent to.
 * @param {string} htmlUrl The URL originating the beacon request.
 * @param {string} data The data to be sent in the POST.
 * @return {boolean} Return true if the request was sent.
 */
pagespeedutils.sendBeacon = function(beaconUrl, htmlUrl, data) {
  var httpRequest;
  // TODO(jud): Use the closure goog.net.Xhrlo.send function here once we have
  // closure lib support in our static JS files.
  if (window.XMLHttpRequest) { // Mozilla, Safari, ...
    httpRequest = new XMLHttpRequest();
  } else if (window.ActiveXObject) { // IE
    try {
      httpRequest = new ActiveXObject('Msxml2.XMLHTTP');
    }
    catch (e) {
      try {
        httpRequest = new ActiveXObject('Microsoft.XMLHTTP');
      }
      catch (e2) {}
    }
  }
  if (!httpRequest) {
    return false;
  }

  // We send the page url in the query param instead of the POST body to assist
  // load balancers or other systems that want to route the beacon based on the
  // originating page.
  // TODO(jud): Handle long URLs correctly. We should send a signal back to the
  // server indicating that we couldn't send the beacon because the URL was too
  // long, so that the server will stop instrumenting pages.
  var query_param_char = beaconUrl.indexOf('?') == -1 ? '?' : '&';
  var url = beaconUrl + query_param_char + 'url=' + encodeURIComponent(htmlUrl);
  httpRequest.open('POST', url);
  httpRequest.setRequestHeader(
      'Content-Type', 'application/x-www-form-urlencoded');
  httpRequest.send(data);
  return true;
};


/**
 * Runs the function when event is triggered.
 * @param {HTMLDocument|Window|Element} elem Element to attach handler.
 * @param {string} eventName Name of the event.
 * @param {function(Event=)} func New onload handler.
 */
pagespeedutils.addHandler = function(elem, eventName, func) {
  if (elem.addEventListener) {
    elem.addEventListener(eventName, func, false);
  } else if (elem.attachEvent) {
    elem.attachEvent('on' + eventName, func);
  } else {
    var oldHandler = elem['on' + eventName];
    elem['on' + eventName] = function() {
      func.call(this);
      if (oldHandler) {
        oldHandler.call(this);
      }
    };
  }
};


/**
 * @param {Node} element dom element.
 * @return {{ top: (number), left: (number) }} The position of the top left
 *     corner of the element when rendered.
 */
pagespeedutils.getPosition = function(element) {
  var top = element.offsetTop;
  var left = element.offsetLeft;

  while (element.offsetParent) {
    element = element.offsetParent;
    top += element.offsetTop;
    left += element.offsetLeft;
  }

  return { top: top, left: left };
};


/**
 * Returns the size of the window.
 * @return {{
 *     height: (number),
 *     width: (number)
 * }}
 */
pagespeedutils.getWindowSize = function() {
  var height = window.innerHeight || document.documentElement.clientHeight ||
      document.body.clientHeight;
  var width = window.innerWidth || document.documentElement.clientWidth ||
      document.body.clientWidth;
  return {
    height: height,
    width: width
  };
};


/**
 * @param {Node} element Dom element.
 * @param {{ height: (number), width: (number) }} windowSize Size of the
 *     window.
 * @return {boolean} true iff some part of element is visible in viewport.
 */
pagespeedutils.inViewport = function(element, windowSize) {
  var position = pagespeedutils.getPosition(element);
  return pagespeedutils.positionInViewport(position, windowSize);
};


/**
 * @param {{ top: (number), left: (number) }} pos Position coordinates.
 * @param {{ height: (number), width: (number) }} windowSize Size of the
 * window.
 * @return {boolean} true iff pos is in viewport.
 */
pagespeedutils.positionInViewport = function(pos, windowSize) {
  return (pos.top < windowSize.height && pos.left < windowSize.width);
};


/**
 * Based on closure's goog.async.AnimationDelay.prototype.getRaf_.
 * @return {?function(function(number)): number} The requestAnimationFrame
 *     function, or null if not available on this browser.
 */
pagespeedutils.getRequestAnimationFrame = function() {
  return window.requestAnimationFrame ||
      window.webkitRequestAnimationFrame ||
      window.mozRequestAnimationFrame ||
      window.oRequestAnimationFrame ||
      window.msRequestAnimationFrame ||
      null;
};

/**
 * @return {number} An integer value representing the number of milliseconds
 *     between midnight, January 1, 1970 and the current time.
 */
pagespeedutils.now = Date.now || (function() {
  // TODO(jud) : replace with goog.now when cleaned up.
  // Unary plus operator converts its operand to a number which in the case of
  // a date is done by calling getTime().
  return +new Date();
});

