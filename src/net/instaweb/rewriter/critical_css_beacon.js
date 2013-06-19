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
 * @fileoverview Code for detecting and sending to the server the critical CSS
 * selectors (selectors applying to any DOM elements on the page) on the client
 * side. To use, this script should be injected right before </body> with a call
 * to pagespeed.criticalCssBeaconInit(...) appended to it.
 *
 * @author jud@google.com (Jud Porter)
 */

// TODO(jud): Reuse and share code with critical_images_beacon.js.

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * Return the CSS selectors which apply to DOM elements that are visible
 * on initial page load.
 * @param {Array.<string>} selectors List of the selectors on the page.
 * @return {Array.<string>} critical selectors list.
 */
pagespeed.computeCriticalSelectors = function(selectors) {
  var critical_selectors = [];
  for (var i = 0; i < selectors.length; ++i) {
    try {
      // If this selector matched any DOM elements, then consider it critical.
      if (document.querySelectorAll(selectors[i]).length > 0) {
        critical_selectors.push(selectors[i]);
      }
    } catch (e) {
      // SYNTAX_ERR is thrown if the browser can't parse a selector (eg, CSS3 in
      // a CSS2.1 browser). Ignore these exceptions.
      // TODO(jud): Consider if continue is the right thing to do here. It may
      // be safer to mark this selector as critical if the browser didn't
      // understand it.
      continue;
    }
  }
  return critical_selectors;
};

pagespeed['computeCriticalSelectors'] = pagespeed.computeCriticalSelectors;

/**
 * @constructor
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options. This is required
 *     to perform the property cache lookup when the beacon is handled by the
 *     sever.
 * @param {string} nonce The nonce send by the server.
 * @param {Array.<string>} selectors List of the selectors on the page.
 */
pagespeed.CriticalCssBeacon = function(beaconUrl, htmlUrl, optionsHash,
                                       nonce, selectors) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.selectors_ = selectors;
};

/**
 * Send the beacon as an AJAX POST request to the server.
 * @param {string} data The data to be sent in the POST.
 * @return {boolean} Return true if the request was sent.
 * @private
 */
pagespeed.CriticalCssBeacon.prototype.sendBeacon_ = function(data) {
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
  var query_param_char = this.beaconUrl_.indexOf('?') == -1 ? '?' : '&';
  var url = this.beaconUrl_ + query_param_char + 'url=' +
      encodeURIComponent(this.htmlUrl_);
  httpRequest.open('POST', url);
  httpRequest.setRequestHeader(
      'Content-Type', 'application/x-www-form-urlencoded');
  httpRequest.send(data);
  return true;
};

/**
 * Check if CSS selectors apply to DOM elements that are visible on initial page
 * load.
 * @private
 */
pagespeed.CriticalCssBeacon.prototype.checkCssSelectors_ = function() {
  // Define the maximum size of a POST that the server will accept. We shouldn't
  // send more data than this.
  // TODO(jud): Factor out this const so that it matches kMaxPostSizeBytes, and
  // set to a smaller size based on typical critical CSS beacon sizes.
  var MAX_DATA_LEN = 131072;

  var critical_selectors = pagespeed.computeCriticalSelectors(this.selectors_);

  var data = 'oh=' + this.optionsHash_ + '&n=' + this.nonce_;
  data += '&cs=';
  for (var i = 0; i < critical_selectors.length; ++i) {
    var tmp = (i > 0) ? ',' : '';
    tmp += encodeURIComponent(critical_selectors[i]);
    // TODO(jud): Don't truncate the critical selectors list if we exceed
    // MAX_DATA_LEN. Either send a signal back that we exceeded the limit, or
    // send multiple beacons back with all the data.
    if ((data.length + tmp.length) > MAX_DATA_LEN) {
      break;
    }
    data += tmp;
  }
  // Export the URL for testing purposes.
  pagespeed['criticalCssBeaconData'] = data;
  // TODO(jud): This beacon should coordinate with the add_instrumentation JS
  // so that only one beacon request is sent if both filters are enabled.
  this.sendBeacon_(data);
};

/**
 * Runs the function when event is triggered.
 * @param {Window|Element} elem Element to attach handler.
 * @param {string} ev Name of the event.
 * @param {function()} func New onload handler.
 *
 * TODO(nikhilmadan): Avoid duplication with the DeferJs code.
 */
pagespeed.addHandler = function(elem, ev, func) {
  if (elem.addEventListener) {
    elem.addEventListener(ev, func, false);
  } else if (elem.attachEvent) {
    elem.attachEvent('on' + ev, func);
  } else {
    var oldHandler = elem['on' + ev];
    elem['on' + ev] = function() {
      func.call(this);
      if (oldHandler) {
        oldHandler.call(this);
      }
    };
  }
};

/**
 * Initialize.
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options.
 * @param {string} nonce The nonce set by the server.
 * @param {Array.<string>} selectors List of the selectors on the page.
 */
pagespeed.criticalCssBeaconInit = function(beaconUrl, htmlUrl, optionsHash,
                                           nonce, selectors) {
  // Verify that the browser supports the APIs we need and bail out early if we
  // don't.
  if (!document.querySelectorAll) {
    return;
  }

  var temp = new pagespeed.CriticalCssBeacon(beaconUrl, htmlUrl, optionsHash,
                                             nonce, selectors);
  // Add event to the onload handler to scan selectors and beacon back which
  // apply to critical elements.
  var beacon_onload = function() {
    // Attempt not to block other onload events on the page by wrapping in
    // setTimeout().
    window.setTimeout(function() {
      temp.checkCssSelectors_();
    }, 0);
  };
  pagespeed.addHandler(window, 'load', beacon_onload);
};

pagespeed['criticalCssBeaconInit'] = pagespeed.criticalCssBeaconInit;
