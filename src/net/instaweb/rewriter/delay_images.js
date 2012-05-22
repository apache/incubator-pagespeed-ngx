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
 * @fileoverview Code for loading high resolution critical images.
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
pagespeed.DelayImages = function() {
};

/**
 * Replaces low resolution image with high resolution image.
 */
pagespeed.DelayImages.prototype.replaceWithHighRes = function() {
  var imgTags = document.getElementsByTagName('img');
   for (var i = 0; i < imgTags.length; ++i) {
     var src = imgTags[i].getAttribute('pagespeed_high_res_src');
     if (src) {
       imgTags[i].setAttribute('src', src);
     }
  }
};

/**
 * Initializes the delay images module.
 */
pagespeed.delayImagesInit = function() {
  var temp = new pagespeed.DelayImages();
  temp.replaceWithHighRes();
};

pagespeed['delayImagesInit'] = pagespeed.delayImagesInit;
