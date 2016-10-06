/*
 * Copyright 2011 Google Inc.
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
 * @fileoverview Code for inlining low resolution critical images.
 * This javascript is part of DelayImages filter.
 *
 * @author pulkitg@google.com (Pulkit Goyal)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];



/**
 * @constructor
 */
pagespeed.DelayImagesInline = function() {
  /**
   * Map of images whose low resolution image is present.
   * @type {Object}
   * @private
   */
  this.inlineMap_ = {};
};


/**
 * Add low res images to inlineMap_.
 * @param {string} url of the image.
 * @param {string} lowResImage low resolution base64 encoded image.
 */
pagespeed.DelayImagesInline.prototype.addLowResImages =
    function(url, lowResImage) {
  this.inlineMap_[url] = lowResImage;
};
pagespeed.DelayImagesInline.prototype['addLowResImages'] =
    pagespeed.DelayImagesInline.prototype.addLowResImages;


/**
 * For given elements, replace src with low res data url by looking up
 * data-pagespeed-high-res-src attribute value in inlineMap_ (does nothing if
 * the latter attribute is absent).
 * @param {!NodeList<!Element>} elements list of DOM elements to check.
 */
pagespeed.DelayImagesInline.prototype.replaceElementSrc =
    function(elements) {
  for (var i = 0; i < elements.length; ++i) {
    var high_res_src = elements[i].getAttribute('data-pagespeed-high-res-src');
    var src = elements[i].getAttribute('src');
    if (high_res_src && !src) {
      var low_res = this.inlineMap_[high_res_src];
      if (low_res) {
        elements[i].setAttribute('src', low_res);
      }
    }
  }
};
pagespeed.DelayImagesInline.prototype['replaceElementSrc'] =
    pagespeed.DelayImagesInline.prototype.replaceElementSrc;


/**
 * Replace images with the low resolution version if available.
 */
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = function() {
  this.replaceElementSrc(document.getElementsByTagName('img'));
  this.replaceElementSrc(document.getElementsByTagName('input'));
};
pagespeed.DelayImagesInline.prototype['replaceWithLowRes'] =
    pagespeed.DelayImagesInline.prototype.replaceWithLowRes;


/**
 * Initializes the inline delay images module.
 */
pagespeed.delayImagesInlineInit = function() {
  var temp = new pagespeed.DelayImagesInline();
  pagespeed['delayImagesInline'] = temp;
};
pagespeed['delayImagesInlineInit'] = pagespeed.delayImagesInlineInit;
