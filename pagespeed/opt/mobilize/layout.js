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
 * Author: jmarantz@google.com (Joshua Marantz)
 */


goog.provide('mob.Layout');

goog.require('goog.array');
goog.require('goog.dom.TagName');
goog.require('goog.object');
goog.require('mob.layoutConstants');
goog.require('mob.layoutUtil');
goog.require('mob.util');
goog.require('mob.util.ElementId');



/**
 * Creates a context for PageSpeed mobile layout.  The layout runs after the
 * DOM is populated with HTML.  If there are outstanding XHR requests at the
 * time the end-of-body JavaScript is run, then we wait until they quiesce
 * before running layout.  If new XHRs occur afterward, then we run the
 * layout algorithm again when they quiesce, to mobilize any new ajax content.
 *
 * @param {!mob.Mob} psMob
 * @constructor
 */
mob.Layout = function(psMob) {
  /**
   * Mobilization context.
   *
   * @private {!mob.Mob}
   */
  this.psMob_ = psMob;

  /**
   * Identifies a set of of element IDs to avoid touching.  This is a member
   * variable so that other JS files can add their special IDs to our list,
   * making it easier to maintain them separately.
   *
   * @private {!Object.<string, boolean>}
   */
  this.dontTouchIds_ = goog.object.createSet(
      mob.util.ElementId.NAV_PANEL, mob.util.ElementId.HEADER_BAR,
      mob.util.ElementId.SPACER, mob.util.ElementId.LOGO_SPAN);

  /**
   * Holds a target width in pixels for usable screen content,
   * determined from document.documentElement.clientWidth, taking into
   * account global padding around the body.
   *
   * @private @const {number}
   */
  this.maxWidth_ = mob.layoutUtil.computeMaxWidth();

  /**
   * Contains a sequence of operations for mobilizing site layout.
   * @private @const {!Array.<!mob.Layout.SequenceStep_>}
   */
  this.sequence_ = mob.Layout.constructSequence_();

  mob.util.consoleLog('window.mob.Layout.maxWidth=' + this.maxWidth_);
};



/**
 * Represents a single step of a multi-step layout operation.
 *
 * The layout transformations are done in a sequence of phases.  We represent
 * these in a vector with a function for each phase and the name of the phase,
 * which helps us keep a progress-bar updated and write console messages
 * showing the timing of each phase.  This struct holds a single step in
 * the sequence.
 *
 * @param {function(this:mob.Layout, !Element)} fn
 * @param {string} description
 * @private @constructor @struct
 */
mob.Layout.SequenceStep_ = function(fn, description) {
  /** @type {function(this:mob.Layout, !Element)} */
  this.functionObj = fn;
  /** @type {string} */
  this.description = description;
};


/**
 * Adds a new DOM id to the set of IDs that we should not mobilize.
 *
 * @param {string} id
 */
mob.Layout.prototype.addDontTouchId = function(id) {
  this.dontTouchIds_[id] = true;
};


/**
 * Executes the multi-step mobile layout transfomation.  This should
 * be called only after all the background image data has been collected.
 */
mob.Layout.prototype.computeAllSizingAndResynthesize = function() {
  if (!document.body) {
    return;
  }
  for (var i = 0; i < this.sequence_.length; ++i) {
    var sequenceStep = this.sequence_[i];
    sequenceStep.functionObj.call(this, document.body);
    this.psMob_.layoutPassDone(sequenceStep.description);
  }
};


/**
 * Returns the number of passes that the layout engine will make over
 * the DOM.  This is used for progress-bar computation.
 *
 * @return {number}
 */
mob.Layout.prototype.getNumberOfPasses = function() {
  return this.sequence_.length;
};


/**
 * Determines whether this element should not be touched.
 *
 * @param {?Element} element
 * @return {boolean}
 */
mob.Layout.prototype.isDontTouchElement = function(element) {
  if (!element) {
    return true;
  }
  var tagName = element.tagName.toUpperCase();
  return (mob.layoutConstants.DONT_TOUCH_TAGS[tagName] ||
          !!(element.id && this.dontTouchIds_[element.id]));
};


/**
 * Calls method fn on every mobilizable child of element.
 *
 * @param {!Element} element
 * @param {function(!Element)} fn
 * @private
 */
mob.Layout.prototype.forEachMobilizableChild_ = function(element, fn) {
  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    if (!this.isDontTouchElement(childElement)) {
      fn.call(this, childElement);
    }
  }
};


/**
 * Makes PRE tags horizontally scrollable.  PRE tags represent a
 * particular challenge because (we assume) they are set to PRE because
 * you cannot reformat them at all, or shrink sub-elements, or anything,
 * so we have to make them scrollable.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.scrollWidePreTags_ = function(element) {
  if ((element.tagName.toUpperCase() == goog.dom.TagName.PRE) ||
      ((element.offsetWidth > this.maxWidth_) &&
       (window.getComputedStyle(element).getPropertyValue('white-space') ==
        'pre'))) {
    mob.layoutUtil.makeHorizontallyScrollable(element);
  }

  this.forEachMobilizableChild_(element,
                                goog.bind(this.scrollWidePreTags_, this));
};


/**
 * Resizes a background image to fit in this.maxWidth_.
 *
 * See also resizeIfTooWide_, which is focused primarily on tables.
 *
 * @param {!Element} element
 * @param {string} image
 * @param {!CSSStyleDeclaration} computedStyle
 * @private
 */
mob.Layout.prototype.resizeBackgroundImage_ = function(element, image,
                                                       computedStyle) {
  var imageSize = this.psMob_.findImageSize(image);
  if (imageSize && imageSize.width && imageSize.height &&
      !mob.layoutUtil.isProbablyASprite(computedStyle)) {
    mob.layoutUtil.resizeBackgroundImage(element, imageSize, computedStyle,
                                         this.maxWidth_);
  }
};


/**
 * Vertically resizes any containers to meet the needs of their children.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.resizeVertically_ = function(element) {
  this.resizeVerticallyAndReturnBottom_(element, 0);
};


/**
 * Computes whether the element is positioned off the screen.
 * @param {!CSSStyleDeclaration} style
 * @return {boolean}
 */
mob.layoutUtil.isOffScreen = function(style) {
  var top = mob.util.pixelValue(style.top);
  var left = mob.util.pixelValue(style.left);
  return (((top != null) && (top < -100)) || ((left != null) && (left < -100)));
};


/**
 * Computes the lowest bottom (highest number) of all the children,
 * and adjusts the height of the div to accommodate the children.
 * Returns the height.
 *
 * @param {!Element} element
 * @param {number} parentTop
 * @return {?number} the bottom y-position of the element after resizing,
 *   or null if no resizing was done.
 * @private
 */
mob.Layout.prototype.resizeVerticallyAndReturnBottom_ = function(element,
                                                                 parentTop) {
  // Determine the top of the current element.  If element isn't a don't-touch
  // element, we will try to recalculate its bottom based on the subelements.
  var topBottom = mob.layoutUtil.findTopAndBottom(element, parentTop);
  if (!topBottom) {
    return null;
  }
  var top = topBottom[0];
  var bottom = topBottom[1];
  if (this.isDontTouchElement(element)) {
    return bottom;
  }
  bottom = top - 1;

  var computedStyle = window.getComputedStyle(element);
  if (!computedStyle) {
    return null;
  }

  // Respect any min-height set on the element.  Note in particular that we will
  // set min-height in this.resizeBackgroundImage_, and we may not be able to
  // find the 'natural' height of the object based on its subelements.
  var minHeight = mob.layoutUtil.computedDimension(computedStyle, 'min-height');
  if (minHeight != null) {
    bottom += minHeight;
  }

  // Iterate over the element's child-elements, scanning for absolutely
  // positioned children, and recursively calling this method to get
  // the calculated bottom.
  var elementBottom = top + element.offsetHeight - 1;
  var hasChildrenWithSizing = false;
  var hasAbsoluteChildren = false;
  var childBottom;
  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    var childComputedStyle = window.getComputedStyle(childElement);
    if (childComputedStyle && (childComputedStyle.position == 'absolute') &&
        !mob.layoutUtil.isOffScreen(childComputedStyle) &&
        (mob.util.pixelValue(childComputedStyle.getPropertyValue('height')) !=
         0) &&
        (childComputedStyle.getPropertyValue('visibility') != 'hidden')) {
      // For some reason, the iframe holding the tweets on
      // stevesouders.com comes up as 'absolute', but does not
      // appear to behave that way.  And it is loaded asynchronously
      // (XHR response???) so that it has a height of 0 at the time
      // that we are doing our vertical resizes.  So our attempts
      // to compute the proper size here are futile -- we get the
      // wrong answer, and our only hope is to leave the element
      // height as 'auto'.
      //
      //
      // Note also that when inspecting the element in chrome dev tools
      // the iframe does not have absolute positioning, so maybe both
      // that and the height get adjusted in response to an event.
      //
      // TODO(jmarantz): try to wake up on DOM mutations and fix
      // the layout.  A problem here is that if the parent div
      // is manually sized by the site developer to incorporate
      // the eventual size of this absolute child, we will shrink
      // it here.
      hasAbsoluteChildren = true;
    }
    childBottom = this.resizeVerticallyAndReturnBottom_(childElement, top);
    if (childBottom != null) {
      hasChildrenWithSizing = true;
      bottom = Math.max(bottom, childBottom);
    }
  }

  if (hasChildrenWithSizing &&
      (computedStyle.getPropertyValue('position') == 'fixed')) {
    // Don't get any further for position-fixed elements, beyond fixing up
    // their children.
    //
    // In our logo resynthesis completely empties the fixed bar at the top,
    // and that bar was causing layout problems because it was relying on
    // a margin -- which we squashed -- to avoid having the fixed bar obscure
    // the content.  In that case, hasChildrenWithSizing==false.
    //
    // However, other sites may have a fixed menu bar which our navigation
    // currently does *not* empty, and contains weird vertical menus which
    // stay permenantly over the sides of the main content.  We have to avoid
    // resizing the fixed parent because that will reserve too much room for
    // it and create a big blank area at the top of the screen.  In this case,
    // hasChildrenWithSizing==true.
    return null;
  }

  // Based on the calculated values from the child-element scan above,
  // vertically resize element.
  var tagName = element.tagName.toUpperCase();
  if (tagName != goog.dom.TagName.BODY) {
    var height = elementBottom - top + 1;

    if (!hasChildrenWithSizing) {
      // Leaf node, such as text or an A tag.  The only time we should respect
      // the CSS sizing here is if it's a sized IMG tag.  Note that IFRAMes are
      // already excluded by this.isDontTouchElement above.
      if ((tagName != goog.dom.TagName.IMG) && (height > 0) &&
          !element.style.backgroundSize) {
        mob.layoutUtil.removeProperty(element, 'height');
        mob.layoutUtil.setPropertyImportant(element, 'height', 'auto');
        if (element.offsetHeight) {
          elementBottom = top + element.offsetHeight;
        }
      }
      bottom = elementBottom;
    } else if (bottom != elementBottom) {
      if (hasAbsoluteChildren) {
        height = bottom - top + 1;
        mob.layoutUtil.setPropertyImportant(element, 'height',
                                            '' + height + 'px');
      } else {
        mob.layoutUtil.setPropertyImportant(element, 'height', 'auto');
      }
    }
  }
  return bottom;
};


/**
 * Shrinks wide foreground images, background images, tables, and
 * other flexible-width tags.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.resizeIfTooWide_ = function(element) {
  // Try to fix lower-level nested nodes that are simply too wide before
  // re-arranging higher-level nodes.
  this.forEachMobilizableChild_(
      element, goog.bind(this.resizeIfTooWide_, this));
  if (element.offsetWidth <= this.maxWidth_) {
    return;
  }

  var tagName = element.tagName.toUpperCase();
  if (tagName == goog.dom.TagName.TABLE) {
    mob.layoutUtil.resizeWideTable(element, this.maxWidth_);
  } else if (tagName == goog.dom.TagName.IMG) {
    mob.layoutUtil.resizeForegroundImage(element, this.maxWidth_);
  } else {
    var images = mob.layoutUtil.findBackgroundImages(element);
    var computedStyle = window.getComputedStyle(element);
    if (images && computedStyle && images.length == 1) {
      // TODO(jmarantz): handle case where there is more than one image.
      this.resizeBackgroundImage_(element, images[0], computedStyle);
    } else if (goog.array.contains(mob.layoutConstants.LAYOUT_CRITICAL,
                                   tagName)) {
      mob.layoutUtil.makeHorizontallyScrollable(element);
    } else if (mob.layoutConstants.FLEXIBLE_WIDTH_TAGS[tagName]) {
      mob.layoutUtil.setPropertyImportant(element, 'max-width', '100%');
      mob.layoutUtil.removeProperty(element, 'width');
    } else {
      mob.util.consoleLog('Punting on resize of ' + tagName +
                          ' which wants to be ' + element.offsetWidth +
                          ' but this.maxWidth_=' + this.maxWidth_);
    }
  }
};


/**
 * Overrides various styles on the DOM, e.g. large margins & padding,
 * percentages on left and top, etc.  This outer function ensures that
 * the body is hidden before recursing through it.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.cleanupStyles_ = function(element) {
  // Temporarily hide the body to allow computed 'width' to reflect a
  // percentage, if it was expressed that way in CSS.  If we leave the
  // body visible, then the computed width comes out as a pixel value.
  // We are trying to eliminate percentage widths to improve the
  // appearance of some that had set percentage widths when laying out
  // for desktop.
  //
  // See the 'Notes' section in
  // https://developer.mozilla.org/en-US/docs/Web/API/Window.getComputedStyle
  //
  // TODO(jmarantz): investigate if there is a better way to do this, as
  // setting the display to 'none' may force a re-render.
  var saveDisplay = document.body.style.display;
  document.body.style.display = 'none';
  this.cleanupStylesHelper_(element);
  document.body.style.display = saveDisplay;
};


/**
 * Helper method for mob.Layout.prototype.cleanupStyles_ that
 * assumes that body-display has been set to 'none' before calling.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.cleanupStylesHelper_ = function(element) {
  mob.layoutUtil.wrapTextOnWhitespace(element);

  this.forEachMobilizableChild_(element, this.cleanupStylesHelper_);

  // After recursing into children, the computed styles on the parent
  // can change, and we need the new ones.
  var computedStyle = window.getComputedStyle(element);

  if (computedStyle) {
    mob.layoutUtil.stripPercentDimensions(element, computedStyle);
    mob.layoutUtil.trimPaddingAndMargins(element, computedStyle);
  }
};


/**
 * Repairs images that were squeezed by the browser resizing algorithms due
 * to our global CSS max-width:100% setting.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.repairDistortedImages_ = function(element) {
  this.forEachMobilizableChild_(element, this.repairDistortedImages_);
  if (element.tagName.toUpperCase() == goog.dom.TagName.IMG) {
    mob.layoutUtil.repairDistortedImages(element);
  }
};


/**
 * Reorders containers with 'float' elements so they are no longer needed.
 * If there are multiple 'float:right' elements, their order is reversed
 * in addition to stripping their float attributes.
 *
 * @param {!Element} element
 * @return {string} the position of the element (fixed, absolute, static...)
 * @private
 */
mob.Layout.prototype.stripFloats_ = function(element) {
  var elementStyle = window.getComputedStyle(element);
  var position = elementStyle.getPropertyValue('position');
  if (position == 'fixed') {
    return 'fixed';
  }
  if (mob.layoutUtil.isPossiblyASlideShow(element)) {
    return position;
  }

  // Contains nodes that we want to reorder in element, putting
  // them at the end of the child-list in reverse order to their
  // accumulation here.
  var reorderNodes = [];
  var previousChild = null;
  var previousChildHasNegativeBottomMargin = false;

  this.forEachMobilizableChild_(element, function(childElement) {
    var childStyle = window.getComputedStyle(childElement);

    // Clean up the children first, because they might pick up 'float'
    // attributes from their parent.  If we clean the float attributes
    // from the parent first, then we won't be able to detect it when
    // testing the children.
    var childPosition = this.stripFloats_(childElement);
    if ((childPosition == 'fixed') ||
        !childStyle ||
        this.isDontTouchElement(childElement)) {
      // do nothing
    } else {
      if ((childPosition == 'absolute') &&
          !mob.layoutUtil.isOffScreen(childStyle)) {
        mob.layoutUtil.setPropertyImportant(childElement, 'position',
                                            'relative');
      }
      var floatStyle = childStyle.getPropertyValue('float');
      var floatRight = (floatStyle == 'right');
      var displayOverride = 'inline-block';

      // One pattern seen on the web is to use a sequence of
      // elements with style="float:right;clear:right;" to make
      // a second column.  On mobile, this won't fly because there
      // likely won't be room for a second column.  However, we
      // don't want to reorder the nodes like a sequence of same-line
      // "float:right"s.  Instead we want to just strip the float.
      if (floatRight && (childStyle.getPropertyValue('clear') == 'right')) {
        floatRight = false;
        displayOverride = 'block';
        if (previousChild && previousChildHasNegativeBottomMargin) {
          mob.layoutUtil.setPropertyImportant(previousChild, 'margin-bottom',
                                              '0');
        }
      }

      if (floatRight || (floatStyle == 'left') ||
          (displayOverride == 'block')) {
        // It won't be effective to call style.removeProperty('float'); when
        // it's computed from CSS rules, but we can explicitly set it to
        // 'none' right on the object, which will override a value in
        // inherited from a class.
        mob.layoutUtil.setPropertyImportant(childElement, 'float', 'none');
        var display = childStyle.getPropertyValue('display');
        if (display != 'none') {
          // TODO(jmarantz): If we have an invisible block that's
          // got a 'float' attribute, then we don't want to make it
          // visible now; we just want to strip the 'float'.
          mob.layoutUtil.setPropertyImportant(childElement, 'display',
                                              displayOverride);
        }
      }
      if (floatRight) {
        reorderNodes.push(childElement);
      }
      previousChild = childElement;
      var marginBottom =
          mob.layoutUtil.computedDimension(childStyle, 'margin-bottom');
      previousChildHasNegativeBottomMargin =
          ((marginBottom != null) && (marginBottom < 0));
    }
  }.bind(this));

  var i, child;
  for (i = reorderNodes.length - 1; i >= 0; --i) {
    child = reorderNodes[i];
    element.removeChild(child);
  }

  for (i = reorderNodes.length - 1; i >= 0; --i) {
    child = reorderNodes[i];
    element.appendChild(child);
  }

  return position;
};


/**
 * Expands layout-columns to the full width of the mobile screen.
 * When a desktop page with multiple columns is transformed into
 * single-column mode, width-constraints can get in the way of using
 * the available space on the phone.  Thus when we are in single
 * column mode, we should remove these constraints.
 *
 * @param {!Element} element
 * @private
 */
mob.Layout.prototype.expandColumns_ = function(element) {
  var children = this.findLayoutChildren_(element);
  // See if a child is positioned to the right or right of it's neighbor.  If
  // not, we can expand it and its children.
  var prevOffsetRight = null;
  for (var i = 0; i < children.length; ++i) {
    var childElement = children[i];
    var next = (i < children.length - 1) ? children[i + 1] : null;
    var offsetRight = childElement.offsetLeft + childElement.offsetWidth;
    if (((prevOffsetRight == null) ||
         (childElement.offsetLeft < prevOffsetRight)) &&
        ((next == null) || (next.offsetLeft < offsetRight))) {
      var style = window.getComputedStyle(childElement);
      if (style) {
        mob.layoutUtil.removeWidthConstraint(childElement, style);
        this.expandColumns_(childElement);
      }
    }

    var attr = element.getAttribute(mob.layoutUtil.NEGATIVE_BOTTOM_MARGIN_ATTR);
    if (attr) {
      element.removeAttribute(mob.layoutUtil.NEGATIVE_BOTTOM_MARGIN_ATTR);
      var computedStyle = window.getComputedStyle(element);
      var height = mob.layoutUtil.computedDimension(computedStyle, 'height');
      if (height != null) {
        mob.layoutUtil.setPropertyImportant(element, 'margin-bottom',
                                            '' + -height + 'px');
      }
    }
    prevOffsetRight = offsetRight;
  }
};


/**
 * Collects the 'interesting' children of an element for layout purposes.
 *
 * @param {!Element} element
 * @return {!Array.<!Element>} The interesting child elements of element.
 * @private
 */
mob.Layout.prototype.findLayoutChildren_ = function(element) {
  var children = [];
  var elementStyle = window.getComputedStyle(element);
  var position = elementStyle.getPropertyValue('position');
  if (position == 'fixed') {
    return children;
  }

  // Make an array of all interesting children and their computed styles.
  this.forEachMobilizableChild_(element, function(childElement) {
    var computedStyle = window.getComputedStyle(childElement);
    var childPosition = computedStyle.getPropertyValue('position');
    if ((childPosition != 'fixed') &&
        (childPosition != 'absolute') ||
        (childElement.offsetWidth != 0)) {
      children.push(childElement);
    }
  });
  return children;
};


/**
 * Holds the sequence of mobilization entry-points.  We declare this as
 * an array rather than as sequential code so that we can compute how
 * many passes there are for progress bar.
 *
 * @private
 * @return {!Array.<!mob.Layout.SequenceStep_>}
 */
mob.Layout.constructSequence_ = function() {
  return [
    new mob.Layout.SequenceStep_(mob.Layout.prototype.scrollWidePreTags_,
                                 'scroll wide pre-tags'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.stripFloats_,
                                 'string floats'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.cleanupStyles_,
                                 'cleanup styles'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.repairDistortedImages_,
                                 'repair distored images'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.resizeIfTooWide_,
                                 'resize if too wide'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.expandColumns_,
                                 'expand columns'),
    new mob.Layout.SequenceStep_(mob.Layout.prototype.resizeVertically_,
                                 'resize vertically')
  ];
};
