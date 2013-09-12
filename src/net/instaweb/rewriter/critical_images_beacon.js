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

goog.require('pagespeedutils');

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
 * @param {boolean} checkRenderedImageSizes The bool to show if resizing is
 *     being done using the rendered dimensions. If yes we capture the rendered
 *     dimensions and send it back with the beacon.
 * @param {string} nonce The nonce sent by the server.
 */
pagespeed.CriticalImagesBeacon = function(beaconUrl, htmlUrl, optionsHash,
    checkRenderedImageSizes, nonce) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.windowSize_ = pagespeedutils.getWindowSize();
  this.checkRenderedImageSizes_ = checkRenderedImageSizes;
  this.imgLocations_ = {};
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
  var scrollX, scrollY;
  // From https://developer.mozilla.org/en-US/docs/DOM/window.scrollX
  scrollX = (window.pageXOffset !== undefined) ? window.pageXOffset :
      (document.documentElement ||
       document.body.parentNode ||
       document.body).scrollLeft;
  scrollY = (window.pageYOffset !== undefined) ? window.pageYOffset :
      (document.documentElement ||
       document.body.parentNode ||
       document.body).scrollTop;

  return {
    top: rect.top + scrollY,
    left: rect.left + scrollX
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
 * Check position of images and input tags and beacon back images that are
 * visible on initial page load.
 * @private
 */
pagespeed.CriticalImagesBeacon.prototype.checkCriticalImages_ = function() {
  // Define the maximum size of a POST that the server will accept. We shouldn't
  // send more data than this.
  // TODO(jud): Factor out this const so that it matches kMaxPostSizeBytes.
  /** @const */ var MAX_DATA_LEN = 131072;

  // List of tags whose elements we will check to see if they are critical.
  var tags = ['img', 'input'];

  var criticalImgs = [];
  // Use an object to store the keys for criticalImgs so that we get a unique
  // list of them.
  var criticalImgsKeys = {};

  for (var i = 0; i < tags.length; ++i) {
    var elements = document.getElementsByTagName(tags[i]);
    for (var j = 0; j < elements.length; ++j) {
      var key = elements[j].getAttribute('pagespeed_url_hash');
      // TODO(jud): Remove the check for getBoundingClientRect below, either by
      // making elLocation_ work correctly if it isn't defined, or updating the
      // user agent whitelist to exclude UAs that don't support it correctly.
      if (key && elements[j].getBoundingClientRect &&
          this.isCritical_(elements[j])) {
        if (!(key in criticalImgsKeys)) {
          criticalImgs.push(key);
          criticalImgsKeys[key] = true;
        }
      }
    }
  }
  var isDataAvailable = false;
  var data = 'oh=' + this.optionsHash_;
  if (this.nonce_) {
    data += '&n=' + this.nonce_;
  }

  if (criticalImgs.length != 0) {
    data += '&ci=' + encodeURIComponent(criticalImgs[0]);
    for (var i = 1; i < criticalImgs.length; ++i) {
      var tmp = ',' + encodeURIComponent(criticalImgs[i]);
      if ((data.length + tmp.length) > MAX_DATA_LEN) {
        break;
      }
      data += tmp;
    }
    isDataAvailable = true;
  }

  // Add rendered image dimensions as a query param to the beacon url.
  if (this.checkRenderedImageSizes_) {
    var tmp = '&rd=' +
        encodeURIComponent(JSON.stringify(this.getImageRenderedMap()));
    if (data.length + tmp.length <= MAX_DATA_LEN) {
      data += tmp;
    }
    isDataAvailable = true;
  }

  // Export the URL for testing purposes.
  pagespeed['criticalImagesBeaconData'] = data;
  if (isDataAvailable) {
    // TODO(jud): This beacon should coordinate with the add_instrumentation JS
    // so that only one beacon request is sent if both filters are enabled.
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data);
  }
};


/**
 * Retrieves the rendered width and height of images.
 * @return {{
 *          renderedWidth: (number|undefined),
 *          renderedHeight: (number|undefined),
 *          originalWidth: (number|undefined),
 *          originalHeight: (number|undefined)
 *          }}
 *
 */
pagespeed.CriticalImagesBeacon.prototype.getImageRenderedMap = function() {
  var renderedImageDimensions = {};
  // TODO(poojatandon): Get elements for 'input' tag with type="image".
  var images = document.getElementsByTagName('img');
  for (var i = 0; i < images.length; ++i) {
    var img = images[i];
    var key = img.getAttribute('pagespeed_url_hash');
    // naturalWidth and naturalHeight is defined for all browsers except in IE
    // versions 8 and before (non HTML5 support).
    // We bail out in case of other browsers or if hash is undefined.
    if (typeof img.naturalWidth == 'undefined' ||
        typeof img.naturalHeight == 'undefined' ||
        typeof key == 'undefined') {
      return renderedImageDimensions;
    }
    if ((typeof(renderedImageDimensions[img.src]) == 'undefined' &&
         img.width > 0 && img.height > 0 &&
         img.naturalWidth > 0 && img.naturalHeight > 0) ||
        (typeof(renderedImageDimensions[img.src]) != 'undefined' &&
         img.width >= renderedImageDimensions[img.src].renderedWidth &&
         img.height >= renderedImageDimensions[img.src].renderedHeight)) {
      renderedImageDimensions[key] = {
        'renderedWidth' : img.width,
        'renderedHeight' : img.height,
        'originalWidth' : img.naturalWidth,
        'originalHeight' : img.naturalHeight
      };
    }
  }
  return renderedImageDimensions;
};


/**
 * Initialize.
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options.
 * @param {boolean} checkRenderedImageSizes The bool to show if resizing is
 *     being done using the rendered dimensions. If yes we capture the rendered
 *     dimensions and send it back with the beacon.
 * @param {string} nonce The nonce sent by the server.
 */
pagespeed.criticalImagesBeaconInit = function(beaconUrl, htmlUrl, optionsHash,
    checkRenderedImageSizes, nonce) {
  var temp = new pagespeed.CriticalImagesBeacon(
      beaconUrl, htmlUrl, optionsHash, checkRenderedImageSizes, nonce);
  // Add event to the onload handler to scan images and beacon back the visible
  // ones.
  var beaconOnload = function() {
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
  pagespeedutils.addHandler(window, 'load', beaconOnload);
};

pagespeed['criticalImagesBeaconInit'] = pagespeed.criticalImagesBeaconInit;
