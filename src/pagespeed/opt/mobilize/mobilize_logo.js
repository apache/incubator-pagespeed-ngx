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


goog.provide('pagespeed.MobLogo');

goog.require('goog.string');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for Pagespeed logo detector.
 * @param {!pagespeed.Mob} psMob
 * @constructor
 */
pagespeed.MobLogo = function(psMob) {
  /**
   * Mobilization context.
   *
   * @private {!pagespeed.Mob}
   */
  this.psMob_ = psMob;

  /**
   * Array of logo candidates.
   * @private {!Array.<pagespeed.MobLogo.LogoRecord>}
   */
  this.candidates_ = [];
};



/**
 * Creates an empty logo record.
 * @constructor
 * @struct
 */
pagespeed.MobLogo.LogoRecord = function() {
  /**
   * Metric of being a logo element. Metric is computed for the elements with
   * size and position within certain ranges, and with an image in its sub-tree
   * or ancestor. Metric value is determined by the number of attributes of
   * this element or its ancestor which have the substring of 'logo' or
   * organization name.
   *
   * @type {number}
   */
  this.metric = -1;
  /** @type {Element} */
  this.logoElement = null;
  /** @type {Element} */
  this.foregroundElement = null;
  /** @type {string} */
  this.foregroundImage = '';
  /** @type {?pagespeed.MobUtil.ImageSource} */
  this.foregroundSource = null;
  /** @type {pagespeed.MobUtil.Rect} */
  this.foregroundRect = null;
  /** @type {Element} */
  this.backgroundElement = null;
  /** @type {?string} */
  this.backgroundImage = null;
  /** @type {pagespeed.MobUtil.Rect} */
  this.backgroundRect = null;
  /** @type {Array.<number>} */
  this.backgroundColor = null;
};


/**
 * Minimum width of an element in the origin site to be considered as the logo.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.MIN_WIDTH_ = 20;


/**
 * Minimum height of an element in the origin site to be considered as the logo.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.MIN_HEIGHT_ = 10;


/**
 * Maximum height of an element in the origin site to be considered as the logo.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.MAX_HEIGHT_ = 400;


/**
 * Minimum number of pixels of an image to be considered as the logo.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.MIN_PIXELS_ = 400;


/**
 * Maximum Y position of the top border for an element in the origin site to
 * be considered as the logo.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.MAX_TOP_ = 6000;


/**
 * Minimum area of an element which is convered by an image. The area is
 * measured from the origin site.
 * @private {number}
 * @const
 */
pagespeed.MobLogo.prototype.RATIO_AREA_ = 0.5;


/**
 * Return 1 if 'logo' is a substring of the file name, with case ignored;
 * otherwise return 0. This method does more test than findLogoString, because
 * based on experiments, I found out that file name may contain false positives.
 * @param {string} str
 * @return {number}
 */
pagespeed.MobLogo.findLogoInFileName = function(str) {
  if (str) {
    str = str.toLowerCase();
    if (str.indexOf('logo') >= 0 &&
        str.indexOf('logout') < 0 &&
        str.indexOf('no_logo') < 0 &&
        str.indexOf('no-logo') < 0) {
      return 1;
    }
  }
  return 0;
};


/**
 * Find foreground image of the logo. This happens after we identify an element
 * which is a potential logo. The foreground image can be inside the sub-tree
 * of the potential element (including the potential element), or can be in
 * an ancestor of the potential element. This method can be configured to
 * search down (in the subtree) or to search up (in the ancestors). This method
 * will not change the seach direction by itself.
 *
 * @param {!Element} element
 * @param {number} minArea
 * @param {boolean} searchDown
 * @return {pagespeed.MobLogo.LogoRecord}
 * @private
 */
pagespeed.MobLogo.prototype.findForeground_ = function(element, minArea,
                                                       searchDown) {
  var rect = pagespeed.MobUtil.boundingRectAndSize(element);
  var validVisibility = (this.psMob_.getVisibility(element) != 'hidden');
  var area = rect.width * rect.height;
  var validDisplay = rect.width > this.MIN_WIDTH_ &&
      rect.height > this.MIN_HEIGHT_ &&
      area > this.MIN_PIXELS_ &&
      rect.top < this.MAX_TOP_ &&
      rect.height < this.MAX_HEIGHT_;

  if (validVisibility && validDisplay && area >= minArea) {
    var source = null;
    var image = null;
    for (var id in pagespeed.MobUtil.ImageSource) {
      image = pagespeed.MobUtil.extractImage(
          element, pagespeed.MobUtil.ImageSource[id]);
      if (image) {
        source = pagespeed.MobUtil.ImageSource[id];

        // If the foreground is IMG tag, and the image has been loaded,
        // use the natural dimension.
        var returnLogo = true;
        if (source == pagespeed.MobUtil.ImageSource.IMG) {
          var imageSize = this.psMob_.findImageSize(element.src);
          if (imageSize) {
            rect.width = imageSize.width;
            rect.height = imageSize.height;
          } else if (element.naturalWidth) {
            // Take the image size from the IMG element, assuming it's
            // been loaded.  Note that C++ will not necessarily load
            // all the images.  However, the IMG tag might be populated
            // correctly anyway.
            rect.width = element.naturalWidth;
            rect.height = element.naturalHeight;
          } else if (!rect.width || !rect.height) {
            // TODO(huibao): Instead of printing a message and punting,
            // continue processing when the image is loaded.
            console.log('Image ' + element.src + ' may be the logo. ' +
                'It has not been loaded so may be missed.');
            returnLogo = false;
          }
          if (returnLogo &&
              (rect.width <= this.MIN_WIDTH_ ||
               rect.height <= this.MIN_HEIGHT_ ||
               rect.height >= this.MAX_HEIGHT_)) {
            returnLogo = false;
          }
        }

        if (returnLogo) {
          var logoRecord = new pagespeed.MobLogo.LogoRecord();
          logoRecord.foregroundImage = image;
          logoRecord.foregroundElement = element;
          logoRecord.foregroundSource = source;
          logoRecord.foregroundRect = rect;
          return logoRecord;
        }
      }
    }
  }

  if (searchDown) {
    for (var child = element.firstChild; child; child = child.nextSibling) {
      var childElement = pagespeed.MobUtil.castElement(child);
      if (childElement != null) {
        var foreground = this.findForeground_(childElement, minArea,
                                              searchDown);
        if (foreground) {
          return foreground;
        }
      }
    }
    return null;
  } else if (element.parentNode) {
    var parentElement = pagespeed.MobUtil.castElement(element.parentNode);
    if (parentElement != null) {
      return this.findForeground_(parentElement, minArea, searchDown);
    }
  }
  return null;
};


/**
 * Find the element that is likely to be a logo. An element can be a logo if
 * it meets conditions (A) and (B).
 * (A) One or more of the following attributes contains 'logo' or the
 *     organization name: (1) title, (2) id, (3) class name, (4) alt,
 *     (5) file name.
 * (B) Has an image within the predefined size range and position range in
 *     its sub-tree or its ancestor. The image can be in <IMG> or <SVG> tag
 *     or as a background image.
 *
 * This method firstly tries to identify a potential element as the logo. If
 * it finds one, it searchs down the tree, starting from the potential element,
 * for the foregound image. If it couldn't find a foreground image from the
 * sub-tree, it searchs up, again starting from the potential element, for the
 * foreground image.
 *
 * @param {!Element} element
 * @param {number} inheritedMetric
 * @return {?pagespeed.MobLogo.LogoRecord}
 * @private
 */
pagespeed.MobLogo.prototype.findLogoNode_ = function(element, inheritedMetric) {
  var rect = pagespeed.MobUtil.boundingRectAndSize(element);
  var validVisibility = (this.psMob_.getVisibility(element) != 'hidden');
  var validDisplay = rect.top < this.MAX_TOP_ &&
      rect.height < this.MAX_HEIGHT_;

  if (!(validDisplay && validVisibility)) {
    return null;
  }

  var metricLogo = 0;
  if (element.title) {
    metricLogo += goog.string.caseInsensitiveContains(element.title, 'logo');
  }
  if (element.id) {
    metricLogo += goog.string.caseInsensitiveContains(element.id, 'logo');
  }
  if (element.className) {
    metricLogo += goog.string.caseInsensitiveContains(element.className,
                                                      'logo');
  }
  if (element.alt) {
    metricLogo += goog.string.caseInsensitiveContains(element.alt, 'logo');
  }

  var org = pagespeed.MobUtil.getSiteOrganization();
  var metricOrg = 0;
  if (org) {
    if (element.id) {
      metricLogo += goog.string.caseInsensitiveContains(element.id, org);
    }
    if (element.className) {
      metricLogo += goog.string.caseInsensitiveContains(element.className, org);
    }
    if (element.title) {
      metricOrg += pagespeed.MobUtil.findPattern(element.title, org);
    }
    if (element.alt) {
      metricOrg += pagespeed.MobUtil.findPattern(element.alt, org);
    }
  }

  var area = rect.width * rect.height;
  var minArea = area * this.RATIO_AREA_;

  // If the element has 'href' and it points to the landing page, the element
  // may be a logo candidate. Typical construct looks like
  //   <a href='...'><img src='...'></a>
  var metricHref = 0;
  if (element.href &&
      element.href == window.location.origin + window.location.pathname) {
    ++metricHref;
  }

  var searchDown = true;
  // Try to seach down in the DOM tree for foreground image.
  var logoRecord = this.findForeground_(element, minArea, searchDown);
  if (!logoRecord) {
    // Now try searching up in the DOM tree.
    logoRecord = this.findForeground_(element, area, !searchDown);
  }

  if (logoRecord) {
    var imageSrc = pagespeed.MobUtil.resourceFileName(
        logoRecord.foregroundImage);

    metricLogo += pagespeed.MobLogo.findLogoInFileName(imageSrc);
    if (imageSrc && org) {
      metricOrg += pagespeed.MobUtil.findPattern(imageSrc, org);
    }

    var metric = metricLogo + metricOrg + metricHref;
    if (metric > 0) {
      logoRecord.metric = metric;
      logoRecord.logoElement = element;
      return logoRecord;
    }
  }

  return null;
};


/**
 * Find all of the logo candidates.
 * @param {!Element} element
 * @param {number} inheritedMetric
 * @private
 */
pagespeed.MobLogo.prototype.findLogoCandidates_ = function(
    element, inheritedMetric) {
  var newCandidate = this.findLogoNode_(element, inheritedMetric);
  if (newCandidate) {
    this.candidates_.push(newCandidate);
    ++inheritedMetric;
  }

  for (var child = element.firstChild; child; child = child.nextSibling) {
    var childElement = pagespeed.MobUtil.castElement(child);
    if (childElement != null) {
      this.findLogoCandidates_(childElement, inheritedMetric);
    }
  }
};


/**
 * Find the best logo candidate, if there is any. The best candidate is the
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
 * @return {?pagespeed.MobLogo.LogoRecord}
 * @private
 */
pagespeed.MobLogo.prototype.findBestLogo_ = function() {
  var logo = null;
  var logoCandidates = this.candidates_;
  if (!logoCandidates || logoCandidates.length == 0) {
    return null;
  }

  if (logoCandidates.length == 1) {
    logo = logoCandidates[0];
    return logo;
  }

  // Use the position and size to update the metric.
  // TODO(huibao): Split the update into a method.
  var maxBot = 0;
  var minTop = Infinity;
  var rect, i, candidate;
  for (i = 0; candidate = logoCandidates[i]; ++i) {
    rect = candidate.foregroundRect;
    minTop = Math.min(minTop, rect.top);
    maxBot = Math.max(maxBot, rect.bottom);
  }
  for (i = 0; candidate = logoCandidates[i]; ++i) {
    rect = candidate.foregroundRect;
    // TODO(huibao): Investigate a better way for incorporating size and
    // position in the selection of the best logo, for example
    // Math.sqrt((maxBot - rect.bottom) / (maxBot - minTop)).
    var multTop = Math.sqrt((maxBot - rect.top) / (maxBot - minTop));
    candidate.metric *= multTop;
  }

  var maxMetric = 0;
  for (i = 0; candidate = logoCandidates[i]; ++i) {
    maxMetric = Math.max(maxMetric, candidate.metric);
  }

  var bestCandidates = [];
  for (i = 0; candidate = logoCandidates[i]; ++i) {
    if (candidate.metric == maxMetric) {
      bestCandidates.push(candidate);
    }
  }

  if (bestCandidates.length == 1) {
    logo = bestCandidates[0];
    return logo;
  }

  // There are multiple candiates with the same largest metric.
  minTop = Infinity;
  var bestLogo = bestCandidates[0];
  var bestRect = bestLogo.foregroundRect;
  for (i = 1; candidate = bestCandidates[i]; ++i) {
    rect = candidate.foregroundRect;
    if (bestRect.top > rect.top ||
        (bestRect.top == rect.top && bestRect.left > rect.left) ||
        (bestRect.top == rect.top && bestRect.left == rect.left &&
         bestRect.width * bestRect.height > rect.width * rect.height)) {
      bestLogo = candidate;
      bestRect = bestLogo.foregroundRect;
    }
  }
  logo = bestLogo;
  return logo;
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
 * Find the background image or color for the logo.
 * @param {pagespeed.MobLogo.LogoRecord} logo
 * @return {pagespeed.MobLogo.LogoRecord}
 * @private
 */
pagespeed.MobLogo.prototype.findLogoBackground_ = function(logo) {
  if (!logo || !logo.foregroundElement) {
    return null;
  }

  var element = logo.foregroundElement;

  // Check the current element.
  var backgroundImage = null;
  if (logo.foregroundSource == pagespeed.MobUtil.ImageSource.IMG ||
      logo.foregroundSource == pagespeed.MobUtil.ImageSource.SVG) {
    backgroundImage =
        pagespeed.MobUtil.extractImage(
        element, pagespeed.MobUtil.ImageSource.BACKGROUND);
  }

  var backgroundColor = this.extractBackgroundColor_(element);
  var parentElement = null;
  if (element.parentNode) {
    parentElement = pagespeed.MobUtil.castElement(element.parentNode);
  }

  // Check the ancestors.
  while (parentElement && !backgroundImage && !backgroundColor) {
    element = parentElement;
    backgroundImage =
        pagespeed.MobUtil.extractImage(
            element, pagespeed.MobUtil.ImageSource.IMG) ||
        pagespeed.MobUtil.extractImage(
            element, pagespeed.MobUtil.ImageSource.SVG) ||
        pagespeed.MobUtil.extractImage(
            element, pagespeed.MobUtil.ImageSource.BACKGROUND);
    backgroundColor = this.extractBackgroundColor_(element);

    if (element.parentNode) {
      parentElement = pagespeed.MobUtil.castElement(element.parentNode);
    } else {
      parentElement = null;
    }
  }

  logo.backgroundElement = element;
  logo.backgroundImage = backgroundImage;
  logo.backgroundColor = backgroundColor || [255, 255, 255];
  logo.backgroundRect = pagespeed.MobUtil.boundingRectAndSize(element);
  return logo;
};


/**
 * Extract theme of the page. This is the entry method.
 * @return {pagespeed.MobLogo.LogoRecord}
 * @export
 */
pagespeed.MobLogo.prototype.run = function() {
  if (!document.body) {
    return null;
  }

  this.findLogoCandidates_(document.body, /* initial metric */ 0);
  var logo = this.findBestLogo_();
  return this.findLogoBackground_(logo);
};
