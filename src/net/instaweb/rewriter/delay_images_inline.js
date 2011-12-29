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
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
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
 * Replace images with the low resolution version if available.
 */
pagespeed.DelayImagesInline.prototype.replaceWithLowRes = function() {
  var imgTags = document.getElementsByTagName('img');
  for (var i = 0; i < imgTags.length; ++i) {
    var src = imgTags[i].getAttribute('pagespeed_high_res_src');
    if (src) {
      imgTags[i].setAttribute('src', this.inlineMap_[src]);
    }
  }
};
pagespeed.DelayImagesInline.prototype['replaceWithLowRes'] =
  pagespeed.DelayImagesInline.prototype.replaceWithLowRes;

/**
 * Initializes the inline delay images module.
 */
pagespeed.delayImagesInlineInit = function() {
  pagespeed.delayImagesInline = new pagespeed.DelayImagesInline();
  pagespeed['delayImagesInline'] = pagespeed.delayImagesInline;
};
pagespeed['delayImagesInlineInit'] = pagespeed.delayImagesInlineInit;
