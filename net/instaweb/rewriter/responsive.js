/*
 * Copyright 2015 Google Inc.
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
 *
 * Author: jmarantz@google.com (Joshua Marantz)
 * Author: sligocki@google.com (Shawn Ligocki)
 */

// TODO(sligocki): Move to third_party/pagespeed/opt/responsive?

goog.provide('pagespeed.Responsive');
goog.provide('pagespeed.ResponsiveImage');
goog.provide('pagespeed.ResponsiveImageCandidate');
goog.provide('pagespeed.responsiveInstance');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.string');



/**
 * A single candidate image URL and target resolution (2x, 4x, etc.) from
 * a responsive image srcset.
 * @struct
 * @constructor
 * @param {number} resolution
 * @param {string} url
 */
pagespeed.ResponsiveImageCandidate = function(resolution, url) {
  /**
   * What devicePixelRatio is this image intended for?
   * @type {number}
   */
  this.resolution = resolution;

  /**
   * URL of image meant for this resolution.
   * @type {string}
   */
  this.url = url;
};



/**
 * Information about each responsive image.
 * @constructor
 * @param {!Element} img
 */
pagespeed.ResponsiveImage = function(img) {
  /**
   * @type {!Element}
   */
  this.img = img;

  /**
   * Current resolution level used as src.
   * @type {number}
   */
  this.currentResolution = 0;

  /**
   * List of possible resolution levels (with corresponding URLs).
   * It must be sorted from lowest to highest resolution level.
   * @type {!Array<!pagespeed.ResponsiveImageCandidate>}
   */
  this.availableResolutions = [];
};



/**
 * @constructor
 */
pagespeed.Responsive = function() {
  /**
   * List of all responsive images on page and the resolutions available for
   * each one. These are all updated on zoom.
   * @private {!Array<!pagespeed.ResponsiveImage>}
   */
  this.allImages_ = [];
};


/**
 * Pre-load hi-res image in background, updating this image src once it's
 * in cache.
 * @param {!Element} img
 * @param {string} url
 * @private
 */
pagespeed.Responsive.updateImgSrc_ = function(img, url) {
  var tempImg = new Image();
  tempImg.onload = function() {
    img.src = url;
  };
  tempImg.src = url;
};


/**
 * Load this image at the appropriate resolution setting. Current algorithm is
 * to load the smallest resolution >= devicePixelRatio.
 *
 * TODO(sligocki): Should we just load the highest resolution as soon as
 * they zoom?
 * TODO(sligocki): Is this the algorithm that browsers would use or do they
 * do something more complicated to compensate for moires, etc.?
 *
 * @param {number} devicePixelRatio
 */
pagespeed.ResponsiveImage.prototype.responsiveResize = function(
    devicePixelRatio) {
  if (devicePixelRatio > this.currentResolution) {
    var numResolutions = this.availableResolutions.length;
    for (var i = 0; i < numResolutions; ++i) {
      if (devicePixelRatio <= this.availableResolutions[i].resolution) {
        this.currentResolution = this.availableResolutions[i].resolution;
        pagespeed.Responsive.updateImgSrc_(this.img,
                                           this.availableResolutions[i].url);
        break;
      }
    }
  }
};


/**
 * Return the actual number of device pixels per CSS pixel including zoom.
 * Note that C-+ resizing on desktops seems to affect window.devicePixelRatio,
 * but pinch zoom on mobile does not seem to affect this value.
 *
 * @return {number}
 */
pagespeed.Responsive.prototype.computeDevicePixelRatioWithZoom = function() {
  var zoomRatio = document.documentElement.clientWidth / window.innerWidth;
  return goog.dom.getPixelRatio() * zoomRatio;
};


/**
 * Resize all images in response to a resize event.
 */
pagespeed.Responsive.prototype.responsiveResize = function() {
  var devicePixelRatio = this.computeDevicePixelRatioWithZoom();
  var numImages = this.allImages_.length;
  for (var i = 0; i < numImages; ++i) {
    this.allImages_[i].responsiveResize(devicePixelRatio);
  }
};


/**
 * Find the index for the first char to match regular expression re in str.
 * If no chars match re, returns str.length.
 *
 * @param {string} str String to search within.
 * @param {!RegExp} re RegExp to search for.
 * @return {number} Smallest index matching re (or str.length if none does).
 * @private
 */
pagespeed.Responsive.search_ = function(str, re) {
  var offset = str.search(re);
  if (offset == -1) {
    return str.length;
  } else {
    return offset;
  }
};


/**
 * From https://html.spec.whatwg.org/#space-character
 * The space characters, for the purposes of this specification, are
 * U+0020 SPACE, U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF),
 * U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR).
 * @private @const {!RegExp}
 */
var WHITESPACE = /[ \t\n\f\r]/;


/** @private @const {!RegExp} */
var NOT_WHITESPACE = /[^ \t\n\f\r]/;


/** @private @const {!RegExp} */
var WHITESPACE_OR_COMMA = /[ \t\n\f\r,]/;


/** @private @const {!RegExp} */
var NOT_WHITESPACE_OR_COMMA = /[^ \t\n\f\r,]/;


/**
 * Parse srcset attribute string into a ResponsiveImage object.
 * @param {!Element} img
 * @param {string} src
 * @param {string} srcset
 * @return {?pagespeed.ResponsiveImage}
 */
pagespeed.Responsive.prototype.parseSrcset = function(img, src, srcset) {
  var respImage = new pagespeed.ResponsiveImage(img);
  var has_1x = false;
  var rest = srcset;

  // Decompose srcset into each resolution
  // Mostly follows:
  // https://html.spec.whatwg.org/multipage/embedded-content.html#parse-a-srcset-attribute
  // with the main exception that we fail early for most situations where the
  // srcset contains w descriptors (or other descriptors we don't understand).

  // Skip whitespace before first candidate URL. Ignore spurious preceding
  // commas too. Although preceding commas are considered parse errors, the
  // spec says to skip them and continue parsing the rest of the srcset.
  var pos = pagespeed.Responsive.search_(rest, NOT_WHITESPACE_OR_COMMA);
  rest = rest.slice(pos);
  while (rest.length > 0) {
    // URL is terminated by either a white space or a comma followed by a space.
    // Note: urlEnd is actually the index after the end of the URL.
    pos = pagespeed.Responsive.search_(rest, WHITESPACE);
    // Note rest[0] was not whitespace nor comma nor EOF, so url >0 length.
    var url = rest.slice(0, pos);
    rest = rest.slice(pos);

    if (url[url.length - 1] == ',') {
      // We cannot deal with srcset with no descriptors.
      // Abort the whole thing.
      return null;
    }

    // Skip whitespace
    pos = pagespeed.Responsive.search_(rest, NOT_WHITESPACE);
    rest = rest.slice(pos);
    // Descriptor is terminated by either a comma or whitespace.
    // Note: According to the spec, descriptor lexing rules are actually more
    // complicated and involve skipping over paren sections, however any such
    // strings (with parentheses) will fail to parse as a number and we will
    // fail the entire parse.
    // Note: descriptorEnd is the index after the end of the descriptor.
    pos = pagespeed.Responsive.search_(rest, WHITESPACE_OR_COMMA);
    var descriptor = rest.slice(0, pos);
    rest = rest.slice(pos);
    if ((descriptor.length > 1) &&
        (descriptor[descriptor.length - 1] == 'x')) {
      var resolution = goog.string.toNumber(descriptor.slice(0, -1));
      if (isNaN(resolution)) {
        return null;
      }
      respImage.availableResolutions.push(
          new pagespeed.ResponsiveImageCandidate(resolution, url));
      if (resolution == 1) {
        has_1x = true;
      }
    } else {
      // We cannot deal with srcset with w (or no) descriptors.
      // Abort the whole thing.
      // TODO(sligocki): Do we want to support srcset w descriptors? Or empty
      // descriptors, spec seems to say empty descriptor -> 1x.
      return null;
    }

    // Skip over whitespace before comma.
    pos = pagespeed.Responsive.search_(rest, NOT_WHITESPACE);
    rest = rest.slice(pos);
    if (rest.length > 0 && rest[0] != ',') {
      // Invalid srcset, should only have one descriptor field before comma or
      // end of string.
      return null;
    } else {
      // Skip over comma.
      rest = rest.slice(1);
    }
    // Skip whitespace after comma (before next candidate).
    pos = pagespeed.Responsive.search_(rest, NOT_WHITESPACE_OR_COMMA);
    rest = rest.slice(pos);
  }

  if (!has_1x && src) {
    // Use src for 1x version if no 1x in srcset.
    respImage.availableResolutions.push(
        new pagespeed.ResponsiveImageCandidate(1, src));
  }

  respImage.availableResolutions.sort(function(a, b) {
    return a.resolution - b.resolution;
  });

  return respImage;
};


/**
 * Collect all responsive images on site, add attributes and event listeners
 * and actually evaluate responsive srcset (as a polyfil).
 */
pagespeed.Responsive.prototype.init = function() {
  // Initialize responsive images.
  var images = goog.dom.getElementsByTagName(goog.dom.TagName.IMG);
  for (var i = 0, img; img = images[i]; ++i) {
    var src = img.getAttribute('src');
    var srcset = img.getAttribute('srcset');
    if (srcset) {
      var respImage = this.parseSrcset(img, src, srcset);
      if (respImage != null) {
        this.allImages_.push(respImage);
      }
    }
  }

  // Set event listeners to resize all images if any zoom event happens.

  // Resize event is fired on desktop C-+/C--, but not mobile pinch zoom.
  window.addEventListener(goog.events.EventType.RESIZE,
                          goog.bind(this.responsiveResize, this));
  // Heuristic for detecting pinch zoom.
  // Detect touchmove with more than one touch.
  // TODO(sligocki): Will touchmove event give us most zoomed view? Or do we
  // need to attach to a touchend event for that?
  // TODO(sligocki): Jud says this will fire continuously, test to see if this
  // will cause too much load on a site with many images and rate limit if it
  // does.
  window.addEventListener(goog.events.EventType.TOUCHMOVE,
                          goog.bind(function(event) {
                            // Multiple fingers.
                            if (event.touches.length > 1) {
                              this.responsiveResize();
                            }
                          }, this));

  // Polyfill (Apply responsive images for any browser which doesn't support
  // srcset natively).
  this.responsiveResize();
};


/**
 * Singleton instance used for keeping track of all responsive image rewrites.
 * @type {pagespeed.Responsive}
 */
pagespeed.responsiveInstance = new pagespeed.Responsive();
pagespeed.responsiveInstance.init();
