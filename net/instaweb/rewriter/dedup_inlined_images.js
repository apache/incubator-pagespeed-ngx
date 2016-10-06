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
 * @fileoverview Code for deduplicating inlined images.
 *
 * @author matterbury@google.com (Matt Atterbury)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 */
pagespeed.DedupInlinedImages = function() {
};

/**
 * Set the immediately preceding img element's src attribute to the value of
 * the identified img element's src attribute, which will be a data URI.
 * @param {string} from_img_id is the id of the img element to copy src= from.
 * @param {string} to_img_id is the id of the img element to copy src= to.
 * @param {string} script_id is the id of the script element invoking us.
 */
pagespeed.DedupInlinedImages.prototype.inlineImg = function(
    from_img_id, to_img_id, script_id) {
  // Find the img to copy from, identified by the given from_img_id.
  var srcNode = document.getElementById(from_img_id);
  if (!srcNode) {
    // console.log('PSA ERROR: from_img_id="' + from_img_id + '": no img');
    return;
  }
  // Find the img to copy to, being the element before the invoking script.
  var dstNode = document.getElementById(to_img_id);
  if (!dstNode) {
    // console.log('PSA ERROR: to_img_id="' + to_img_id + '": no img');
    return;
  }
  // Find the invoking script.
  var scriptNode = document.getElementById(script_id);
  if (!scriptNode) {
    // console.log('PSA ERROR: script_id="' + script_id + '": no script');
    return;
  }
  // Copy the src attribute then delete this script.
  dstNode.src = srcNode.getAttribute('src');
  scriptNode.parentNode.removeChild(scriptNode);
};

pagespeed.DedupInlinedImages.prototype['inlineImg'] =
    pagespeed.DedupInlinedImages.prototype.inlineImg;

/**
 * Initializes the dedup inlined images module.
 */
pagespeed.dedupInlinedImagesInit = function() {
  var dedupInlinedImages = new pagespeed.DedupInlinedImages();
  pagespeed['dedupInlinedImages'] = dedupInlinedImages;
};

pagespeed['dedupInlinedImagesInit'] = pagespeed.dedupInlinedImagesInit;
