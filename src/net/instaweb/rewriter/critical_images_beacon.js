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
 */
pagespeed.CriticalImagesBeacon = function(beaconUrl, htmlUrl, optionsHash) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.windowSize_ = this.getWindowSize_();
  this.imgLocations_ = {};
};

/**
 * Returns the size of the window.
 * @return {{
 *     height: (number),
 *     width: (number)
 * }}
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.getWindowSize_ = function() {
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
 * Returns the absolute position of the top left corner of the element.
 * @param {Element} element DOM element to calculate the location of.
 * @return {{
 *      top: (number),
 *      left: (number)
 * }}
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.elLocation_ = function(element) {
  var rect = element.getBoundingClientRect();

  // getBoundingClientRect() is w.r.t. the viewport. Add the amount scolled to
  // calculate the absolute position of the element.
  var scroll_x, scroll_y;
  // From https://developer.mozilla.org/en-US/docs/DOM/window.scrollX
  scroll_x = (window.pageXOffset !== undefined) ? window.pageXOffset :
      (document.documentElement ||
       document.body.parentNode ||
       document.body).scrollLeft;
  scroll_y = (window.pageYOffset !== undefined) ? window.pageYOffset :
      (document.documentElement ||
       document.body.parentNode ||
       document.body).scrollTop;

  return {
    top: rect.top + scroll_y,
    left: rect.left + scroll_x
  };
};

/**
 * Returns true if an element is visible upon initial page load.
 * @param {Element} element The DOM element to check for visibility.
 * @return {boolean} True if the element is critical.
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.isCritical_ = function(element) {
  // TODO(jud): We can perform a more efficient critical image check if lazyload
  // images is enabled, and this beacon code runs after the lazyload JS has
  // initially executed. Specifically, we know an image is not critical if it
  // still has the 'pagespeed_lazy_src' attribute, meaning that the image was
  // not visible in the viewport yet. This will save us potentially many calls
  // to the expensive getBoundingClientRect().

  // Make sure the element is visible first before checking its position on the
  // page. Note, this check works correctly with the lazyload placeholder image,
  // since that image is a 1x1 pixel, and styling it display=none also sets
  // offsetWidth and offsetHeight to 0.
  if (element.offsetWidth <= 0 && element.offsetHeight <= 0) {
    return false;
  }

  var elLocation = this.elLocation_(element);
  // Only return 1 image as critical if there are multiple images that have the
  // same location. This is to handle sliders with many images in the same
  // location, but most of which only appear after onload.
  var elLocationStr = elLocation.top.toString() + ',' +
      elLocation.left.toString();
  if (this.imgLocations_.hasOwnProperty(elLocationStr)) {
    return false;
  } else {
    this.imgLocations_[elLocationStr] = true;
  }

  return (elLocation.top <= this.windowSize_.height &&
          elLocation.left <= this.windowSize_.width);
};

/**
 * Send the beacon as an AJAX POST request to the server.
 * @param {string} data The data to be sent in the POST.
 * @return {boolean} Return true if the request was sent.
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.sendBeacon_ = function(data) {
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
 * Check position of images and input tags and beacon back images that are
 * visible on initial page load.
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.checkCriticalImages_ = function() {
  // Define the maximum size of a POST that the server will accept. We shouldn't
  // send more data than this.
  // TODO(jud): Factor out this const so that it matches kMaxPostSizeBytes.
  var MAX_DATA_LEN = 131072;

  // List of tags whose elements we will check to see if they are critical.
  var tags = ['img', 'input'];

  var critical_imgs = [];
  // Use an object to store the keys for critical_imgs so that we get a unique
  // list of them.
  var critical_imgs_keys = {};

  for (var i = 0; i < tags.length; ++i) {
    var elements = document.getElementsByTagName(tags[i]);
    for (var j = 0; j < elements.length; ++j) {
      var key = elements[j].getAttribute('pagespeed_url_hash');
      // TODO(jud): Remove the check for getBoundingClientRect below, either by
      // making elLocation_ work correctly if it isn't defined, or updating the
      // user agent whitelist to exclude UAs that don't support it correctly.
      if (key && elements[j].getBoundingClientRect &&
          this.isCritical_(elements[j])) {
        if (!(key in critical_imgs_keys)) {
          critical_imgs.push(key);
          critical_imgs_keys[key] = true;
        }
      }
    }
  }

  if (critical_imgs.length != 0) {
    var data = 'oh=' + this.optionsHash_;
    data += '&ci=' + encodeURIComponent(critical_imgs[0]);
    for (var i = 1; i < critical_imgs.length; ++i) {
      var tmp = ',' + encodeURIComponent(critical_imgs[i]);
      if ((data.length + tmp.length) > MAX_DATA_LEN) {
        break;
      }
      data += tmp;
    }
    // Export the URL for testing purposes.
    pagespeed['criticalImagesBeaconData'] = data;
    // TODO(jud): This beacon should coordinate with the add_instrumentation JS
    // so that only one beacon request is sent if both filters are enabled.
    this.sendBeacon_(data);
  }
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
 */
pagespeed.criticalImagesBeaconInit = function(beaconUrl, htmlUrl, optionsHash) {
  var temp = new pagespeed.CriticalImagesBeacon(
      beaconUrl, htmlUrl, optionsHash);
  // Add event to the onload handler to scan images and beacon back the visible
  // ones.
  var beacon_onload = function() {
    // Attempt not to block other onload events on the page by wrapping in
    // setTimeout().
    // TODO(jud): checkCriticalImages_ should not run until after lazyload
    // images completes. This will allow us to reduce the complexity of managing
    // the interaction between the beacon and the lazyload jS, and to do a more
    // efficient check for image visibility.
    window.setTimeout(function() {
      temp.checkCriticalImages_();
    }, 0);
  };
  pagespeed.addHandler(window, 'load', beacon_onload);
};

pagespeed['criticalImagesBeaconInit'] = pagespeed.criticalImagesBeaconInit;
