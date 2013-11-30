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

goog.provide('pagespeed.CriticalImages');

goog.require('goog.dom.TagName');
goog.require('pagespeedutils');



/**
 * Code for detecting and sending to server the critical images (images above
 * the fold) on the client side.
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl URL of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options. This is required
 *     to perform the property cache lookup when the beacon is handled by the
 *     sever.
 * @param {boolean} checkRenderedImageSizes The bool to show if resizing is
 *     being done using the rendered dimensions. If yes we capture the rendered
 *     dimensions and send it back with the beacon.
 * @param {string} nonce The nonce sent by the server.
 * @constructor
 * @private
 */
pagespeed.CriticalImages.Beacon_ = function(
    beaconUrl, htmlUrl, optionsHash, checkRenderedImageSizes, nonce) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  /** @private  {{height: number, width: number}} */
  this.windowSize_ = pagespeedutils.getWindowSize();
  this.checkRenderedImageSizes_ = checkRenderedImageSizes;
  /** @private {!Object.<string, boolean>} */
  this.imgLocations_ = {};
};


/**
 * Returns the absolute position of the top left corner of the element.
 * @param {!Element} element DOM element to calculate the location of.
 * @return {{top: number, left: number}}
 * @private
 */
pagespeed.CriticalImages.Beacon_.prototype.elLocation_ = function(element) {
  var rect = element.getBoundingClientRect();

  // getBoundingClientRect() is w.r.t. the viewport. Add the amount scrolled to
  // calculate the absolute position of the element.
  // From https://developer.mozilla.org/en-US/docs/DOM/window.scrollX
  var body = document.body;
  var scrollX = 'pageXOffset' in window ? window.pageXOffset :
      (document.documentElement || body.parentNode || body).scrollLeft;
  var scrollY = 'pageYOffset' in window ? window.pageYOffset :
      (document.documentElement || body.parentNode || body).scrollTop;

  return {
    top: rect.top + scrollY,
    left: rect.left + scrollX
  };
};


/**
 * Returns true if an element is critical, meaning it is visible upon initial
 * page load.
 * @param {!Element} element The DOM element to check for visibility.
 * @return {boolean}
 * @private
 */
pagespeed.CriticalImages.Beacon_.prototype.isCritical_ = function(element) {
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
  // Only return one image as critical if there are multiple images that have
  // the same location. This is to handle sliders with many images in the same
  // location, but most of which only appear after onload.
  var elLocationStr = elLocation.top.toString() + ',' + elLocation.left;
  if (this.imgLocations_.hasOwnProperty(elLocationStr)) {
    return false;
  } else {
    this.imgLocations_[elLocationStr] = true;
  }

  return elLocation.top <= this.windowSize_.height &&
         elLocation.left <= this.windowSize_.width;
};


/**
 * Checks the position of images and input tags and beacons back images that are
 * visible on initial page load.
 * @private
 */
pagespeed.CriticalImages.Beacon_.prototype.checkCriticalImages_ = function() {
  // List of tags whose elements we will check to see if they are critical.
  var tags = [goog.dom.TagName.IMG, goog.dom.TagName.INPUT];

  var criticalImgs = [];
  // Use an object to store the keys for criticalImgs so that we get a unique
  // list of them.
  var criticalImgsKeys = {};

  for (var i = 0; i < tags.length; ++i) {
    var elements = document.getElementsByTagName(tags[i]);
    for (var j = 0, element; element = elements[j]; ++j) {
      var key = element.getAttribute('pagespeed_url_hash');
      // TODO(jud): Remove the check for getBoundingClientRect below, either by
      // making elLocation_ work correctly if it isn't defined, or updating the
      // user agent whitelist to exclude UAs that don't support it correctly.
      if (key && element.getBoundingClientRect && this.isCritical_(element) &&
          !(key in criticalImgsKeys)) {
        criticalImgs.push(key);
        criticalImgsKeys[key] = true;
      }
    }
  }
  var data = 'oh=' + this.optionsHash_;
  if (this.nonce_) {
    data += '&n=' + this.nonce_;
  }

  var isDataAvailable = criticalImgs.length != 0;
  if (isDataAvailable) {
    data += '&ci=' + encodeURIComponent(criticalImgs[0]);
    for (var i = 1; i < criticalImgs.length; ++i) {
      var tmp = ',' + encodeURIComponent(criticalImgs[i]);
      if (data.length + tmp.length <= pagespeedutils.MAX_POST_SIZE) {
        data += tmp;
      }
    }
  }

  // Add rendered image dimensions as a query param to the beacon URL.
  if (this.checkRenderedImageSizes_) {
    var tmp = '&rd=' +
        encodeURIComponent(JSON.stringify(this.getImageRenderedMap()));
    if (data.length + tmp.length <= pagespeedutils.MAX_POST_SIZE) {
      data += tmp;
    }
    isDataAvailable = true;
  }

  // Export the URL for testing purposes.
  pagespeed.CriticalImages.beaconData_ = data;
  if (isDataAvailable) {
    // TODO(jud): This beacon should coordinate with the add_instrumentation JS
    // so that only one beacon request is sent if both filters are enabled.
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data);
  }
};


/**
 * Retrieves the rendered width and height of images.
 * @return {{
 *     renderedWidth: (number|undefined),
 *     renderedHeight: (number|undefined),
 *     originalWidth: (number|undefined),
 *     originalHeight: (number|undefined)
 * }}
 *
 */
pagespeed.CriticalImages.Beacon_.prototype.getImageRenderedMap = function() {
  var renderedImageDimensions = {};
  // TODO(poojatandon): Get elements for 'input' tag with type="image".
  var images = document.getElementsByTagName(goog.dom.TagName.IMG);
  for (var i = 0, img; img = images[i]; ++i) {
    var key = img.getAttribute('pagespeed_url_hash');
    // naturalWidth and naturalHeight is defined for all browsers except in IE
    // versions 8 and before (non HTML5 support).
    // We bail out in case of other browsers or if hash is undefined.
    if (!('naturalWidth' in img) || !('naturalHeight' in img) ||
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


/** @private string */
pagespeed.CriticalImages.beaconData_ = '';


/**
 * Gets the data sent in the beacon after pagespeed.CriticalImages.Run()
 * completes. Used to verify that the beacon ran correctly in tests.
 * @return {string}
 * @export
 */
pagespeed.CriticalImages.getBeaconData = function() {
  return pagespeed.CriticalImages.beaconData_;
};


/**
 * Scans images and beacons back the visible ones at the onload event.
 * @param {string} beaconUrl The URL on the server to send the beacon to.
 * @param {string} htmlUrl URL of the page the beacon is being inserted on.
 * @param {string} optionsHash The hash of the rewrite options.
 * @param {boolean} checkRenderedImageSizes The bool to show if resizing is
 *     being done using the rendered dimensions. If yes we capture the rendered
 *     dimensions and send it back with the beacon.
 * @param {string} nonce The nonce sent by the server.
 * @export
 */
pagespeed.CriticalImages.Run = function(
    beaconUrl, htmlUrl, optionsHash, checkRenderedImageSizes, nonce) {
  var beacon = new pagespeed.CriticalImages.Beacon_(
      beaconUrl, htmlUrl, optionsHash, checkRenderedImageSizes, nonce);
  var beaconOnload = function() {
    // Attempt not to block other onload events on the page by wrapping in
    // setTimeout().
    // TODO(jud): checkCriticalImages_ should not run until after lazyload
    // images completes. This will allow us to reduce the complexity of managing
    // the interaction between the beacon and the lazyload jS, and to do a more
    // efficient check for image visibility.
    window.setTimeout(function() { beacon.checkCriticalImages_(); }, 0);
  };
  pagespeedutils.addHandler(window, 'load', beaconOnload);
};
