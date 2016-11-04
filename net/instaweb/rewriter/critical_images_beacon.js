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

goog.require('goog.array');
goog.require('goog.dom');
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

  // TODO(jud): Consider using goog.structs.Set instead of using an array + map
  // combination below.
  /**
   * Array of critical image URL hash keys.
   * @private {!Array.<string>}
   */
  this.criticalImages_ = [];
  /**
   * Object used to store the keys from this.criticalImages_ so that we get a
   * unique list of them.
   * @private {!Object.<string, boolean>}
   */
  this.criticalImagesKeys_ = {};
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
  // still has the 'data-pagespeed-lazy-src' attribute, meaning that the image
  // was not visible in the viewport yet. This will save us potentially many
  // calls to the expensive getBoundingClientRect().

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
 * Inserts the image key string into criticalImages_ and criticalImagesKeys_
 * if it is critical (visible).
 * @param {!Element} element The DOM element to check for visibility.
 * @private
 */
pagespeed.CriticalImages.Beacon_.prototype.insertIfImageIsCritical_ =
    function(element) {
  var key = element.getAttribute('data-pagespeed-url-hash');
  if (key && !(key in this.criticalImagesKeys_) &&
      this.isCritical_(element)) {
    this.criticalImages_.push(key);
    this.criticalImagesKeys_[key] = true;
  }
};


/**
 * Checks position of image element on onload of the image to decide whether
 * it is visible or not and adds it to a map if critical.
 * @param {!Element} element The DOM element to check for visibility.
 */
pagespeed.CriticalImages.Beacon_.prototype.checkImageForCriticality =
    function(element) {
  // TODO(jud): Remove the check for getBoundingClientRect below, either by
  // making elLocation_ work correctly if it isn't defined, or updating the
  // user agent whitelist to exclude UAs that don't support it correctly.
  if (element.getBoundingClientRect) {
    this.insertIfImageIsCritical_(element);
  }
};


/**
 * Checks position of image element on onload of the image to decide whether
 * it is visible or not and adds it to a map if critical.
 * @param {Element} element The DOM element to check for visibility.
 * @export
 */
pagespeed.CriticalImages.checkImageForCriticality = function(element) {
  pagespeed.CriticalImages.beaconObj_.checkImageForCriticality(element);
};


/**
 * Check all images to see if they are critical and beacon when finished. Should
 * be called either at page onload, or when the onload handler for all images
 * has finished running if lazyload_images is enabled.
 * @export
 */
pagespeed.CriticalImages.checkCriticalImages = function() {
  pagespeed.CriticalImages.beaconObj_.checkCriticalImages_();
};


/**
 * Check position of images and input tags and beacon back all visible images.
 * This method is triggered on page onload and goes over all image elements
 * available at this point, and merges this set with the set of visible image
 * elements detected via image onload logic (via checkImageForCriticality).
 * Images detected at image-onload time are more accurate in detecting initial
 * images of slideshow-like features.
 * @private
 */
pagespeed.CriticalImages.Beacon_.prototype.checkCriticalImages_ = function() {
  // Start with a fresh imgLocations_ map so that anything that did not get a
  // chance to get detected at image-onload time because of accidental overlap
  // between image locations will now have a chance to get identified. Note that
  // in case of slideshows, this can cause duplicate images to be detected, but
  // the correct first slideshow image would already have been detected at image
  // onload time.
  this.imgLocations_ = {};
  // Generate a list of the elements that can be considered critical.
  var tags = [goog.dom.TagName.IMG, goog.dom.TagName.INPUT];
  var elemsToCheck = [];
  for (var i = 0; i < tags.length; ++i) {
    elemsToCheck = elemsToCheck.concat(
        goog.array.toArray(document.getElementsByTagName(tags[i])));
  }

  // Return early if there aren't any items to check.
  if (elemsToCheck.length == 0) { return; }

  // Verify that the browser supports all the features we need, and bail out if
  // it doesn't.
  // TODO(jud): Remove the check for getBoundingClientRect, either by making
  // elLocation_ work correctly if it isn't defined, or updating the user agent
  // whitelist to exclude UAs that don't support it correctly.
  if (!elemsToCheck[0].getBoundingClientRect) { return; }

  for (var i = 0, element; element = elemsToCheck[i]; ++i) {
    this.insertIfImageIsCritical_(element);
  }
  var data = 'oh=' + this.optionsHash_;
  if (this.nonce_) {
    data += '&n=' + this.nonce_;
  }

  var isDataAvailable = this.criticalImages_.length != 0;
  if (isDataAvailable) {
    data += '&ci=' + encodeURIComponent(this.criticalImages_[0]);
    for (var i = 1; i < this.criticalImages_.length; ++i) {
      var tmp = ',' + encodeURIComponent(this.criticalImages_[i]);
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
 * Retrieves the rendered width and height of images. The returned object uses
 * abbreviated key names (rw = rendered width, oh = original height, etc.) to
 * keep the data sent back in the beacon compact.
 * @return {!Object.<string, {
 *     rw: number,
 *     rh: number,
 *     ow: number,
 *     oh: number}>} Object mapping an image's data-pagespeed-url-hash to its
 *     original and rendered widths and heights.
 */
pagespeed.CriticalImages.Beacon_.prototype.getImageRenderedMap = function() {
  var renderedImageDimensions = {};
  // TODO(poojatandon): Get elements for 'input' tag with type="image". This
  // currently doesn't work because input tags don't support naturalWidth and
  // naturalHeight.
  var images = goog.dom.getElementsByTagName(goog.dom.TagName.IMG);
  if (images.length == 0) { return {}; }

  // naturalWidth and naturalHeight is defined for all browsers except in IE
  // versions 8 and before (non HTML5 support).
  var img = images[0];
  if (!('naturalWidth' in img) || !('naturalHeight' in img)) { return {}; }

  for (var i = 0; img = images[i]; ++i) {
    var key = img.getAttribute('data-pagespeed-url-hash');
    if (!key) { continue; }
    if ((!(key in renderedImageDimensions) &&
             img.width > 0 && img.height > 0 &&
             img.naturalWidth > 0 && img.naturalHeight > 0) ||
        ((key in renderedImageDimensions) &&
             img.width >= renderedImageDimensions[key].rw &&
             img.height >= renderedImageDimensions[key].rh)) {
      renderedImageDimensions[key] = {
        'rw' : img.width,
        'rh' : img.height,
        'ow' : img.naturalWidth,
        'oh' : img.naturalHeight
      };
    }
  }
  // TODO(poojatandon): Some background images are larger than the view port,
  // especially on mobile devices. In such cases it would be better to resize
  // (or crop) the background image to the viewport size, because this would cut
  // down the image size significantly.  But we don't do this yet.
  return renderedImageDimensions;
};


/** @private string */
pagespeed.CriticalImages.beaconData_ = '';


/** @private Object Beacon object */
pagespeed.CriticalImages.beaconObj_;


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
 * @param {boolean} sendBeaconAtOnload Controls whether the beacon should be
 *     sent at page onload. This should be set to false if lazyload is also
 *     enabled, since we want to wait to check all images on the page until they
 *     have finished running their onload handlers.
 * @param {boolean} checkRenderedImageSizes The bool to show if resizing is
 *     being done using the rendered dimensions. If yes we capture the rendered
 *     dimensions and send it back with the beacon.
 * @param {string} nonce The nonce sent by the server.
 * @export
 */
pagespeed.CriticalImages.Run = function(
    beaconUrl, htmlUrl, optionsHash, sendBeaconAtOnload,
    checkRenderedImageSizes, nonce) {
  var beacon = new pagespeed.CriticalImages.Beacon_(
      beaconUrl, htmlUrl, optionsHash, checkRenderedImageSizes, nonce);
  pagespeed.CriticalImages.beaconObj_ = beacon;
  // If lazyload is enabled on the page, then it will handle calling the beacon
  // when all images have finished loading. Otherwise, call at onload when all
  // images are loaded.
  if (sendBeaconAtOnload) {
    var beaconOnload = function() {
      // Attempt not to block other onload events on the page by wrapping in
      // setTimeout().
      window.setTimeout(function() { beacon.checkCriticalImages_(); }, 0);
    };
    pagespeedutils.addHandler(window, 'load', beaconOnload);
  }
};
