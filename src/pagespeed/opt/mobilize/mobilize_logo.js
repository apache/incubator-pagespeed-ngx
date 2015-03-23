/*
 * Copyright 2014 Google Inc.
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
 * Author: huibao@google.com (Huibao Lin)
 */


// Steps for detecting logo and computing theme color.
// 1: Find out all elements which may be logo. An element is a logo candidate
//    if it has the 'logo' string or the organization name in its attributes,
//    or has HREF pointing to the landing URL.
// 2: Find out all images which may be the foreground images of the logo. For
//    each of the logo elements, we consider all images (IMG, SVG, and
//    background image) in its descedants, and the background image in its
//    nearest ancestor.
//    Wait until all of these images loaded before doing the next steps.
// 3. Remove the candidates which do not have images of proper size and
//    position.
// 4. Find out the best candidate element, considering image size, position, and
//    the number of attributes with 'logo' or organization string.
// 5. Find out the background color of the best candidate element. This is the
//    non-transparent color of its nearest ancestor.
// 6. Compute theme color based on the background color and foreground image.

goog.provide('pagespeed.MobLogo');
goog.provide('pagespeed.MobLogoCandidate');

goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.string');
goog.require('pagespeed.MobColor');
goog.require('pagespeed.MobUtil');



/**
 * @constructor
 * @struct
 * @param {pagespeed.MobLogo.LogoRecord} logoRecord
 * @param {!goog.color.Rgb} background
 * @param {!goog.color.Rgb} foreground
 */
pagespeed.MobLogoCandidate = function(logoRecord, background, foreground) {
  /** {pagespeed.MobLogo.LogoRecord} */
  this.logoRecord = logoRecord;

  /** {!goog.color.Rgb} */
  this.background = background;

  /** {!goog.color.Rgb} */
  this.foreground = foreground;
};



/**
 * Creates a context for Pagespeed logo detector.
 * @param {!pagespeed.Mob} psMob
 * @param {function(!Array.<pagespeed.MobLogoCandidate>)} doneCallback
 * @param {number} maxNumCandidates
 * @constructor
 */
pagespeed.MobLogo = function(psMob, doneCallback, maxNumCandidates) {
  /**
   * Mobilization context.
   *
   * @private {!pagespeed.Mob}
   */
  this.psMob_ = psMob;

  /**
   * Callback to invoke when this object finishes its work.
   * @private {function(!Array.<pagespeed.MobLogoCandidate>)} doneCallback_
   */
  this.doneCallback_ = doneCallback;

  /** @private {?string} */
  this.organization_ = pagespeed.MobUtil.getSiteOrganization();

  /** @private {string} */
  this.landingUrl_ = window.location.origin + window.location.pathname;

  /**
   * Array of logo candidates.
   * @private {!Array.<!pagespeed.MobLogo.LogoRecord>}
   */
  this.candidates_ = [];

  /** @private {number} */
  this.pendingEventCount_ = 0;

  /** @private {number} */
  this.maxNumCandidates_ = maxNumCandidates;
};



/**
 * Creates an empty logo record.
 * @param {number} metric
 * @param {!Element} element
 * @constructor @struct
 */
pagespeed.MobLogo.LogoRecord = function(metric, element) {
  /**
   * Metric of being a logo element. Metric is computed for the elements with
   * size and position within certain ranges, and with an image in its sub-tree
   * or ancestor. Metric value is determined by the number of attributes of
   * this element or its ancestor which have the substring of 'logo' or
   * organization name.
   *
   * @type {number}
   */
  this.metric = metric;
  /** @type {!Element} */
  this.logoElement = element;
  /** @type {Array.<Element>} */
  this.childrenElements = [];
  /** @type {Array.<Element>} */
  this.childrenImages = [];
  /** @type {Element} */
  this.ancestorElement = null;
  /** @type {Element} */
  this.ancestorImage = null;
  /** @type {Element} */
  this.foregroundElement = null;
  /** @type {Element} */
  this.foregroundImage = element;
  /** @type {pagespeed.MobUtil.Rect} */
  this.rect = null;
  /** @type {Array.<number>} */
  this.backgroundColor = null;
};


/**
 * Minimum width of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
pagespeed.MobLogo.prototype.MIN_WIDTH_ = 20;


/**
 * Minimum height of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
pagespeed.MobLogo.prototype.MIN_HEIGHT_ = 10;


/**
 * Maximum height of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
pagespeed.MobLogo.prototype.MAX_HEIGHT_ = 400;


/**
 * Minimum area of an element which must be covered by an image for that image
 * to be considered a logo.
 * @private @const {number}
 */
pagespeed.MobLogo.prototype.RATIO_AREA_ = 0.5;


/**
 * Find the element that is likely to be a logo.
 * @param {!Element} element Element being tested whether has logo attributes
 * @return {?pagespeed.MobLogo.LogoRecord}
 * @private
 */
pagespeed.MobLogo.prototype.findLogoElement_ = function(element) {
  if (this.psMob_.getVisibility(element) == 'hidden') {
    return null;
  }

  // Find URL of the image. Some sites use 'logo' or the organization name
  // to name the logo image, so the file name is a signal for identifying the
  // logo. However, such signal is not available for inlined images.
  var imageSrc = null;
  if (element.nodeName.toUpperCase() == goog.dom.TagName.IMG) {
    imageSrc = element.src;
  } else {
    // IMG tag can also have background image, but it's ignored for now until
    // we see actual use of it.
    imageSrc = pagespeed.MobUtil.findBackgroundImage(element);
  }
  imageSrc = pagespeed.MobUtil.resourceFileName(imageSrc);
  if (imageSrc.indexOf('data:image/') != -1) {
    imageSrc = null;
  }

  var signals =
      [element.title, element.id, element.className, element.alt, imageSrc];

  var metric = 0;
  var i;
  for (i = 0; i < signals.length; ++i) {
    if (signals[i]) {
      if (goog.string.caseInsensitiveContains(signals[i], 'logo')) {
        ++metric;
      }
    }
  }
  if (this.organization_) {
    for (i = 0; i < signals.length; ++i) {
      if (signals[i] &&
          pagespeed.MobUtil.findPattern(signals[i], this.organization_)) {
        ++metric;
      }
    }
  }

  // If the element has 'href' and it points to the landing page, the element
  // may be a logo candidate. Typical construct looks like
  //   <a href='...'><img src='...'></a>
  if (element.href == this.landingUrl_) {
    ++metric;
  }

  if (metric > 0) {
    return (new pagespeed.MobLogo.LogoRecord(metric, element));
  }

  return null;
};


/**
 * Find all of the logo candidates.
 * @param {!Element} element
 * @private
 */
pagespeed.MobLogo.prototype.findLogoCandidates_ = function(element) {
  var newCandidate = this.findLogoElement_(element);
  if (newCandidate) {
    this.candidates_.push(newCandidate);
  }

  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    this.findLogoCandidates_(childElement);
  }
};


/**
 * Add the image to the pending list. We will wait until all pending images
 * have been loaded before finding the best logo.
 * @param {!Element} img
 * @private
 */
pagespeed.MobLogo.prototype.addImageToPendingList_ = function(img) {
  ++this.pendingEventCount_;
  img.addEventListener(goog.events.EventType.LOAD,
                       goog.bind(this.eventDone_, this));
  img.addEventListener(goog.events.EventType.ERROR,
                       goog.bind(this.eventDone_, this));
};


/**
 * Create a new IMG tag using the specified image source. This IMG tag will be
 * monitored.
 * @param {string} imageSrc
 * @return {!Element}
 * @private
 */
pagespeed.MobLogo.prototype.newImage_ = function(imageSrc) {
  var img = document.createElement(goog.dom.TagName.IMG);
  this.addImageToPendingList_(img);
  img.src = imageSrc;
  return img;
};


/**
 * Collect all images in the element's descendants.
 * @param {!Element} element
 * @param {Array.<Element>} childrenElements
 * @param {Array.<Element>} childrenImages
 * @private
 */
pagespeed.MobLogo.prototype.collectChildrenImages_ = function(
    element, childrenElements, childrenImages) {
  var imageSrc = null;
  for (var src in pagespeed.MobUtil.ImageSource) {
    imageSrc = pagespeed.MobUtil.extractImage(
        element, pagespeed.MobUtil.ImageSource[src]);
    if (imageSrc) {
      var img = null;
      if (src == pagespeed.MobUtil.ImageSource.IMG) {
        img = element;
        if (!element.naturalWidth) {
          this.addImageToPendingList_(img);
        }
      } else {
        img = this.newImage_(imageSrc);
      }
      childrenElements.push(element);
      childrenImages.push(img);
      // Ignore background image of IMG and SVG tags.
      break;
    }
  }


  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    this.collectChildrenImages_(childElement, childrenElements, childrenImages);
  }
};


/**
 * Find all images which may be the foreground of the logo.
 * @param {!Array.<pagespeed.MobLogo.LogoRecord>} logoCandidates
 * @private
 */
pagespeed.MobLogo.prototype.findImagesAndWait_ = function(logoCandidates) {
  for (var i = 0; i < logoCandidates.length; ++i) {
    var logo = logoCandidates[i];
    var element = logo.logoElement;
    this.collectChildrenImages_(element, logo.childrenElements,
                                logo.childrenImages);

    // Find the background in the logo element's nearest ancestor.
    if (element.parentNode) {
      element = pagespeed.MobUtil.castElement(element.parentNode);
    } else {
      element = null;
    }
    while (element) {
      var imageSrc = pagespeed.MobUtil.findBackgroundImage(element);
      if (imageSrc) {
        logo.ancestorElement = element;
        logo.ancestorImage = this.newImage_(imageSrc);
        break;
      }
      element = element.parentElement;
    }
  }

  if (this.pendingEventCount_ == 0) {
    this.findBestLogoAndColor_();
  }
};


/**
 * @param {Array.<Object>} array
 * @param {number} index
 * @private
 */
pagespeed.MobLogo.fastRemoveArrayElement_ = function(array, index) {
  var last = array.length - 1;
  if (index < last) {
    array[index] = array[last];
  }
  array.pop();
};


/**
 * Remove the logo candidates which do not have image of proper size and
 * position.
 * @private
 */
pagespeed.MobLogo.prototype.pruneCandidateBySizePos_ = function() {
  var logoCandidates = this.candidates_;
  for (var i = 0; i < logoCandidates.length; ++i) {
    var logo = logoCandidates[i];
    var element = logo.logoElement;
    var rect = pagespeed.MobUtil.boundingRectAndSize(element);
    var area = rect.width * rect.height;

    var minArea = area * this.RATIO_AREA_;
    var bestIndex = -1;
    var bestImageArea = 0;
    for (var j = 0; j < logo.childrenElements.length; ++j) {
      var img = logo.childrenElements[j];
      if (!img) {
        continue;
      }
      rect = pagespeed.MobUtil.boundingRectAndSize(img);
      area = rect.width * rect.height;
      if (area >= minArea && rect.width > this.MIN_WIDTH_ &&
          rect.height > this.MIN_HEIGHT_ && rect.height < this.MAX_HEIGHT_) {
        if (area > bestImageArea) {
          bestImageArea = area;
          bestIndex = j;
        }
      }
    }

    if (bestIndex >= 0) {
      logo.foregroundElement = logo.childrenElements[bestIndex];
      logo.foregroundImage = logo.childrenImages[bestIndex];
      logo.rect = rect;
    } else if (logo.ancestorElement) {
      img = logo.ancestorElement;
      rect = pagespeed.MobUtil.boundingRectAndSize(img);
      area = rect.width * rect.height;
      if (area >= minArea && rect.width > this.MIN_WIDTH_ &&
          rect.height > this.MIN_HEIGHT_ && rect.height < this.MAX_HEIGHT_) {
        logo.foregroundElement = logo.ancestorElement;
        logo.foregroundImage = logo.ancestorImage;
        logo.rect = rect;
      } else {
        pagespeed.MobLogo.fastRemoveArrayElement_(logoCandidates, i);
        --i;
      }
    } else {
      pagespeed.MobLogo.fastRemoveArrayElement_(logoCandidates, i);
      --i;
    }
  }
};


/**
 * Find the best logo and compute theme color.
 * @private
 */
pagespeed.MobLogo.prototype.findBestLogoAndColor_ = function() {
  this.pruneCandidateBySizePos_();
  var logos = this.findBestLogos_();
  var candidates = [];
  var numCandidates = Math.min(this.maxNumCandidates_, logos.length);
  for (var i = 0; i < numCandidates; ++i) {
    var img = null;
    var background = null;
    var logo = logos[i];
    this.findLogoBackground_(logo);
    img = logo.foregroundImage;
    background = logo.backgroundColor;
    var mobColor = new pagespeed.MobColor();
    var themeColor = mobColor.run(img, background);
    candidates.push(new pagespeed.MobLogoCandidate(
        logo, themeColor.background, themeColor.foreground));
  }
  this.doneCallback_(candidates);
};


/**
 * @private
 */
pagespeed.MobLogo.prototype.eventDone_ = function() {
  --this.pendingEventCount_;
  if (this.pendingEventCount_ == 0) {
    this.findBestLogoAndColor_();
  }
};


/**
 * @private {Array.<Function.<pagespeed.MobUtil.Rect>>}
 */
pagespeed.MobLogo.rectAccessors_ = [
  /**
   * @return {number}
   * @param {pagespeed.MobUtil.Rect} rect
   */
  function(rect) { return rect.top; },

  /**
   * @return {number}
   * @param {pagespeed.MobUtil.Rect} rect
   */
  function(rect) { return rect.left; },

  /**
   * @return {number}
   * @param {pagespeed.MobUtil.Rect} rect
   */
  function(rect) { return rect.width * rect.height; }
];


/**
 * @param {!pagespeed.MobLogo.LogoRecord} a
 * @param {!pagespeed.MobLogo.LogoRecord} b
 * @return {number}
 * @private
 */
pagespeed.MobLogo.compareLogos_ = function(a, b) {
  if (a.metric > b.metric) {    // Higher is better.
    return -1;
  } else if (b.metric > a.metric) {
    return 1;
  }

  for (var i = 0; i < pagespeed.MobLogo.rectAccessors_; ++i) {
    var accessor = pagespeed.MobLogo.rectAccessors_[i];
    var aval = accessor(a.rect);
    var bval = accessor(b.rect);
    if (aval < bval) {
      return -1;
    } else if (bval < aval) {
      return 1;
    }
  }
  return 0;                     // a tie!
};


/**
 * Find up to 5 best logo candidates in rank order. The best candidate is the
 * one with the largest metric value. If there are more than one candiates
 * with the same largest metric, the follow rules are applied on them in order
 * for choosing the best one:
 *   - the candidate with the highest top border
 *   - the candidate with the smallest left border
 *   - the candidate with the largest size
 *
 * If there are still multiple candidates after these rules, then the first
 * one which was found will be chosen.
 *
 * If there are no logo candidates then null is returned.
 *
 * @return {Array.<!pagespeed.MobLogo.LogoRecord>}
 * @private
 */
pagespeed.MobLogo.prototype.findBestLogos_ = function() {
  var logoCandidates = this.candidates_;
  if (logoCandidates.length > 1) {
    // Use the position and size to update the metric.
    // TODO(huibao): Split the update into a method.
    var maxBot = 0;
    var minTop = Infinity;
    var i, rect, candidate;
    for (i = 0; candidate = logoCandidates[i]; ++i) {
      rect = candidate.rect;
      minTop = Math.min(minTop, rect.top);
      maxBot = Math.max(maxBot, rect.bottom);
    }
    for (i = 0; candidate = logoCandidates[i]; ++i) {
      rect = candidate.rect;
      // TODO(huibao): Investigate a better way for incorporating size and
      // position in the selection of the best logo, for example
      // Math.sqrt((maxBot - rect.bottom) / (maxBot - minTop)).
      var multTop = Math.sqrt((maxBot - rect.top) / (maxBot - minTop));
      candidate.metric *= multTop;
    }

    if ((logoCandidates.length > 0) && (this.maxNumCandidates_ == 1)) {
      // Just pick the best one, which is faster than sorting.
      var bestLogo = logoCandidates[0];
      for (i = 1; i < logoCandidates.length; ++i) {
        candidate = logoCandidates[i];
        if (pagespeed.MobLogo.compareLogos_(candidate, bestLogo) < 0) {
          bestLogo = candidate;
        }
      }
      logoCandidates[0] = bestLogo;
    } else {
      logoCandidates.sort(pagespeed.MobLogo.compareLogos_);
    }
  }
  return logoCandidates;
};


/**
 * Extract background color.
 * @param {!Element} element
 * @return {Array.<number>}
 * @private
 */
pagespeed.MobLogo.prototype.extractBackgroundColor_ = function(element) {
  var computedStyle = document.defaultView.getComputedStyle(element, null);
  if (computedStyle) {
    var colorString = computedStyle.getPropertyValue('background-color');
    if (colorString) {
      var colorValues = pagespeed.MobUtil.colorStringToNumbers(colorString);
      if (colorValues &&
          (colorValues.length == 3 ||
           (colorValues.length == 4 && colorValues[3] != 0))) {
        // colorValue should be in RGB format (3 element-array) or RGBA format
        // (4 element-array). If it is in RGBA format and the last element is 0,
        // this color is fully transparent and should be ignored.
        return colorValues;
      }
    }
  }
  return null;
};


/**
 * Find the background color for the logo.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @private
 */
pagespeed.MobLogo.prototype.findLogoBackground_ = function(logo) {
  if (!logo || !logo.foregroundElement) {
    return;
  }

  var backgroundColor = null;
  var element = logo.foregroundElement;
  while (element && !backgroundColor) {
    backgroundColor = this.extractBackgroundColor_(element);
    element = element.parentElement;
  }

  logo.backgroundColor = backgroundColor;
};


/**
 * Extract theme of the page. This is the entry method.
 * @export
 */
pagespeed.MobLogo.prototype.run = function() {
  if (!document.body) {
    return;
  }

  this.findLogoCandidates_(document.body);
  this.findImagesAndWait_(this.candidates_);
};
