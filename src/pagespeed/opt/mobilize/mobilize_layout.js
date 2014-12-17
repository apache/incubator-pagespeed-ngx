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


goog.provide('pagespeed.MobLayout');

goog.require('goog.dom');
goog.require('goog.string');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for PageSpeed mobile layout.  The layout runs after the
 * DOM is populated with HTML.  If there are outstanding XHR requests at the
 * time the end-of-body JavaScript is run, then we wait until they quiesce
 * before running layout.  If new XHRs occur afterward, then we run the
 * layout algorithm again when they quiesce, to mobilize any new ajax content.
 *
 * @param {!pagespeed.Mob} psMob
 * @constructor
 */
pagespeed.MobLayout = function(psMob) {
  /**
   * Mobilization context.
   *
   * @private {pagespeed.Mob}
   */
  this.psMob_ = psMob;

  /**
   * Set of element IDs to avoid touching.  This is a member variable so that
   * other JS files can add their special IDs to our list, making it easier
   * to maintain them separately.
   *
   *
   * @private {Object.<string, boolean>}
   */
  this.dontTouchIds_ = {};

  /**
   * Target width in pixels for usable screen content, determined from
   * document.documentElement.clientWidth, taking into account global padding
   * around the body.
   *
   * @private {number}
   * @const
   */
  this.maxWidth_ = this.computeMaxWidth_();

  console.log('window.pagespeed.MobLayout.maxWidth=' + this.maxWidth_);
};


/**
 * List of style attributes that we want to clamp to 4px max.
 * @private {Array.<string>}
 * @const
 */
pagespeed.MobLayout.CLAMPED_STYLES_ = [
  'padding-left',
  'padding-bottom',
  'padding-right',
  'padding-top',
  'margin-left',
  'margin-bottom',
  'margin-right',
  'margin-top',
  'border-left-width',
  'border-bottom-width',
  'border-right-width',
  'border-top-width',
  'left',
  'top'
];


/**
 * HTML tag names (upper-cased) that should be treated as flexible in
 * width.  This means that if their css-width is specified as being
 * too wide for our screen, we'll override it to 'auto'.
 *
 * @private {Object.<string, boolean>}
 */
pagespeed.MobLayout.FLEXIBLE_WIDTH_TAGS_ = {
  'A': true,
  'DIV': true,
  'FORM': true,
  'H1': true,
  'H2': true,
  'H3': true,
  'H4': true,
  'P': true,
  'SPAN': true,
  'TBODY': true,
  'TD': true,
  'TFOOT': true,
  'TH': true,
  'THEAD': true,
  'TR': true
};


/**
 * List of attributes for which we want to remove any percentage specs
 * @private {Array.<string>}
 * @const
 */
pagespeed.MobLayout.NO_PERCENT_ = [
  'left',
  'width'
];


/**
 * Marker for elements with negative bottom margin.
 * @private {string}
 */
pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_ =
    'data-pagespeed-negative-bottom-margin';


/**
 * Adds a new DOM id to the set of IDs that we should not mobilize.
 * @param {string} id
 */
pagespeed.MobLayout.prototype.addDontTouchId = function(id) {
  this.dontTouchIds_[id] = true;
};


/**
 * Calculates the maximum width we want for elements on the page.
 * @return {number}
 * @private
 */
pagespeed.MobLayout.prototype.computeMaxWidth_ = function() {
  var width = document.documentElement.clientWidth;
  if (width) {
    var bodyStyle = window.getComputedStyle(document.body);
    var extraWidth = ['padding-left', 'padding-right'];
    for (var i = 0; i < extraWidth.length; ++i) {
      var value = pagespeed.MobUtil.computedDimension(bodyStyle, extraWidth[i]);
      if (value) {
        width -= value;
      }
    }
  } else {
    width = 400;
  }
  return width;
};


/**
 * Determines whether this element should not be touched.
 * @param {Element} element
 * @return {boolean}
 * @private
 */
pagespeed.MobLayout.prototype.dontTouch_ = function(element) {
  if (!element) {
    return true;
  }
  var tagName = element.tagName.toUpperCase();
  return ((tagName == 'SCRIPT') ||
          (tagName == 'STYLE') ||
          (tagName == 'IFRAME') ||
          (element.id && this.dontTouchIds_[element.id]) ||
          element.classList.contains('psmob-nav-panel') ||
          element.classList.contains('psmob-header-bar') ||
          element.classList.contains('psmob-header-spacer-div') ||
          element.classList.contains('psmob-logo-span'));
};


/**
 * Returns an Element if this node is (a) an element and (b) is mobilizable
 * -- this is -- does not have an id or class, or tag-name that we've
 * excluded from mobilization.
 *
 * Returns null for non-element nodes and for disqualified elements.
 *
 * @param {!Node} node
 * @return {?Element}
 */
pagespeed.MobLayout.prototype.getMobilizeElement = function(node) {
  var element = pagespeed.MobUtil.castElement(node);
  if (this.dontTouch_(element)) {
    return null;
  }
  return element;
};


/**
 * Returns an array of the element's children, including only those that are
 * actually Elements, excluding text-nodes.  Use this to help loop over
 * children, potentially removing them from the parent or adding new
 * ones from inside the loop.
 *
 * Note that non-element child nodes (e.g. text-nodes) will not be included
 * in the vector.
 *
 * @param {!Element} element
 * @return {!Array.<!Element>}
 * @private
 */
pagespeed.MobLayout.prototype.childElements_ = function(element) {
  var children = [];
  for (var child = element.firstChild; child; child = child.nextSibling) {
    var childElement = pagespeed.MobUtil.castElement(child);
    if (childElement != null) {
      children.push(child);
    }
  }
  return children;
};


/**
 * Returns the element-children of an element as a vector.  Use this to help
 * loop over children, potentially removing them from the parent or adding new
 * ones from inside the loop.
 *
 * Note that non-element child nodes (e.g. text-nodes) will not be included
 * in the vector.
 *
 * @param {!Element} element
 * @param {!Function} fn
 * @private
 */
pagespeed.MobLayout.prototype.forEachMobilizableChild_ = function(element, fn) {
  for (var child = element.firstChild; child; child = child.nextSibling) {
    var childElement = this.getMobilizeElement(child);
    if (childElement != null) {
      fn.call(this, child);
    }
  }
};


/**
 * Returns the number of passes that the layout engine will make over
 * the DOM.  This is used for progress-bar computation.
 *
 * @return {number}
 */
pagespeed.MobLayout.numberOfPasses = function() {
  // sequence_.length will always be a multiple of 2.
  return pagespeed.MobLayout.sequence_.length / 2;
};


/**
 * Resizes huge background images, add scrolling to 'pre' tags.
 *
 * See also resizeIfTooWide_, which is focused primarily on tables.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.shrinkWideElements_ = function(element) {
  var computedStyle = window.getComputedStyle(element);
  var image = pagespeed.MobUtil.findBackgroundImage(element);
  if (image) {
    var img = this.psMob_.findImgTagForUrl(image);
    var style = '';
    if (img && img.width && img.height) {
      var height = img.height;
      var repeat = computedStyle.getPropertyValue('background-repeat');

      if (img.width > this.maxWidth_) {
        var shrinkage = this.maxWidth_ / img.width;
        height = Math.round(img.height * shrinkage);

        var styles = 'background-size:' + this.maxWidth_ + 'px ' +
            height + 'px;background-repeat:no-repeat;';

        // If the element was previously sized exactly to the div, then resize
        // the height of the div to match the new height of the background.
        var elementHeight = pagespeed.MobUtil.computedDimension(
            computedStyle, 'height');
        if (img.height == elementHeight) {
          styles += 'height:' + height + 'px;';
        }
        pagespeed.MobUtil.addStyles(element, styles);
      }
      // Whether or not we are not width-contraining the background image, we
      // give it a height constraint for the benefit of auto-sizing parent
      // nodes.  Note that we look specifically for 'min-height' in
      // resizeVerticallyAndReturnBottom_, so this is both a signal to the
      // browser and to a later pass.
      pagespeed.MobUtil.setPropertyImportant(
          element, 'min-height', '' + height + 'px');
    }
  }

  // TODO(jmarantz): there are a variety of other tagNames that we should
  // allow to scroll as well.
  if ((element.tagName.toUpperCase() == 'PRE') ||
      (computedStyle.getPropertyValue('white-space') == 'pre') &&
      (element.offsetWidth > this.maxWidth_)) {
    element.style.overflowX = 'scroll';
  }

  this.forEachMobilizableChild_(element, this.shrinkWideElements_);
};


/**
 * Main worker for function for mobilization.  This should be called only
 * after all the background image data has been collected.
 */
pagespeed.MobLayout.prototype.computeAllSizingAndResynthesize = function() {
  if (document.body != null) {
    for (var i = 0; i < pagespeed.MobLayout.sequence_.length; ++i) {
      pagespeed.MobLayout.sequence_[i].call(this, document.body);
      ++i;
      this.psMob_.layoutPassDone(pagespeed.MobLayout.sequence_[i]);
    }
  }
};


/**
 * Makes an element be horizontally scrollable.
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.makeHorizontallyScrollable_ = function(element) {
  pagespeed.MobUtil.setPropertyImportant(element, 'overflow-x', 'auto');
  pagespeed.MobUtil.setPropertyImportant(element, 'width', 'auto');
  pagespeed.MobUtil.setPropertyImportant(element, 'display', 'block');
};


/**
 * Vertically resizes any containers to meet the needs of their children.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.resizeVertically_ = function(element) {
  this.resizeVerticallyAndReturnBottom_(element, 0);
};


/**
 * Computes the lowest bottom (highest number) of all the children,
 * and adjusts the height of the div to accomodate the children.
 * Returns the height.
 *
 * @param {!Element} element
 * @param {number} parentTop
 * @return {?number} the bottom y-position of the element after resizing.
 * @private
 */
pagespeed.MobLayout.prototype.resizeVerticallyAndReturnBottom_ =
    function(element, parentTop) {
  var top;
  var bottom;
  var boundingBox = pagespeed.MobUtil.boundingRect(element);
  if (boundingBox) {
    top = boundingBox.top;
    bottom = boundingBox.bottom;
  } else {
    top = parentTop;
    if (element.offsetParent == element.parentNode) {
      top += element.offsetTop;
    } else if (element.offsetParent != element.parentNode.parentNode) {
      return null;
    }
    bottom = top + element.offsetHeight - 1;
  }

  if (this.dontTouch_(element)) {
    return bottom;
  }
  bottom = top - 1;

  var computedStyle = window.getComputedStyle(element);
  if (!computedStyle) {
    return null;
  }

  var minHeight = pagespeed.MobUtil.computedDimension(
      computedStyle, 'min-height');
  if (minHeight != null) {
    bottom += minHeight;
  }

  var elementBottom = top + element.offsetHeight - 1;
  var hasChildrenWithSizing = false;
  var hasAbsoluteChildren = false;
  var childBottom;

  for (var next, child = element.firstChild; child; child = child.nextSibling) {
    var childElement = pagespeed.MobUtil.castElement(child);
    if (childElement) {
      var childComputedStyle = window.getComputedStyle(childElement);
      var absolute = false;
      if (childComputedStyle) {
        var position = childComputedStyle.position;
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
        if ((position == 'absolute') &&
            (childComputedStyle.getPropertyValue('height') != '0px') &&
            (childComputedStyle.getPropertyValue('visibility') != 'hidden')) {
          hasAbsoluteChildren = true;
          absolute = true;
        }
      }
      childBottom = this.resizeVerticallyAndReturnBottom_(childElement, top);
      if (childBottom != null) {
        hasChildrenWithSizing = true;
        bottom = Math.max(bottom, childBottom);
      }
    }
  }

  if (computedStyle.getPropertyValue('position') == 'fixed') {
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
    if (hasChildrenWithSizing) {
      return null;
    }
  }

  var tagName = element.tagName.toUpperCase();
  if (tagName != 'BODY') {
    var height = elementBottom - top + 1;

    if (!hasChildrenWithSizing) {
      // Leaf node, such as text or an A tag.  The only time we should respect
      // the CSS sizing here is if it's a sized IMG tag.  Note that IFRAMes are
      // already excluded by this.getMobilizeElement above.
      if ((tagName != 'IMG') && (height > 0) &&
          (element.style.backgroundSize == '')) {
        pagespeed.MobUtil.removeProperty(element, 'height');
        pagespeed.MobUtil.setPropertyImportant(element, 'height', 'auto');
        if (element.offsetHeight) {
          elementBottom = top + element.offsetHeight;
        }
      }
      bottom = elementBottom;
    } else if (bottom != elementBottom) {
      if (hasAbsoluteChildren) {
        height = bottom - top + 1;
        pagespeed.MobUtil.setPropertyImportant(
            element, 'height', '' + height + 'px');
      } else {
        pagespeed.MobUtil.setPropertyImportant(element, 'height', 'auto');
      }
    }
  }
  return bottom;
};


/**
 * See also shrinkWideElements_, which is focused on images and pre tags.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.resizeIfTooWide_ = function(element) {
  // Try to fix lower-level nested nodes that are simply too wide before
  // re-arranging higher-level nodes.
  var children = this.childElements_(element);
  for (var i = 0; i < children.length; ++i) {
    this.resizeIfTooWide_(children[i]);
  }

  if (element.offsetWidth <= this.maxWidth_) {
    return;
  }

  var tagName = element.tagName.toUpperCase();
  if (tagName == 'TABLE') {
    if (this.isDataTable_(element)) {
      this.makeHorizontallyScrollable_(element);
    } else if (pagespeed.MobUtil.possiblyInQuirksMode()) {
      this.reorganizeTableQuirksMode_(element, this.maxWidth_);
    } else {
      this.reorganizeTableNoQuirksMode_(element, this.maxWidth_);
    }
  } else {
    var image = null;
    var width = element.offsetWidth;
    var height = element.offsetHeight;
    var type = 'img';
    if (tagName == 'IMG') {
      image = element.getAttribute('src');
    } else {
      type = 'background-image';
      image = pagespeed.MobUtil.findBackgroundImage(element);
      var img = (image == null) ? null :
          this.psMob_.findImgTagForUrl(image);
      if (img) {
        width = img.width;
        height = img.height;
      }
    }
    if (image != null) {
      var shrinkage = width / this.maxWidth_;
      if (shrinkage > 1) {
        var newHeight = height / shrinkage;
        console.log('Shrinking ' + type + ' ' + image + ' from ' +
            width + 'x' + height + ' to ' +
            this.maxWidth_ + 'x' + newHeight);
        if (tagName == 'IMG') {
          pagespeed.MobUtil.setPropertyImportant(
              element, 'width', '' + this.maxWidth_ + 'px');
          pagespeed.MobUtil.setPropertyImportant(
              element, 'height', '' + newHeight + 'px');
        } else {
          // http://www.w3schools.com/cssref/css3_pr_background-size.asp
          //
          // See also
          // http://css-tricks.com/how-to-resizeable-background-image/
          // for an alternative.
          pagespeed.MobUtil.setPropertyImportant(
              element, 'background-size',
              '' + this.maxWidth_ + 'px ' + newHeight + 'px');
        }
      }
    } else {
      if ((tagName == 'CODE') ||
          (tagName == 'PRE') ||
          (tagName == 'UL')) {
        this.makeHorizontallyScrollable_(element);
      } else if (pagespeed.MobLayout.FLEXIBLE_WIDTH_TAGS_[tagName]) {
        pagespeed.MobUtil.setPropertyImportant(
            element, 'max-width', '100%');
        pagespeed.MobUtil.removeProperty(element, 'width');
      } else {
        console.log('Punting on resize of ' + tagName +
            ' which wants to be ' + element.offsetWidth +
            ' but this.maxWidth_=' +
            this.maxWidth_);
      }
    }
  }
};


/**
 * Counts the number of container-like objects.  This is used for a heuristic
 * to differentiate data-tables from layout-tables.
 *
 * @param {!Element} element
 * @return {number}
 * @private
 */
pagespeed.MobLayout.prototype.countContainers_ = function(element) {
  var ret = 0;
  var tagName = element.tagName.toUpperCase();
  if ((tagName == 'DIV') || (tagName == 'TABLE') ||
      (tagName == 'UL')) {
    ++ret;
  }
  for (var child = element.firstChild; child; child = child.nextSibling) {
    var childElement = pagespeed.MobUtil.castElement(child);
    if (childElement != null) {
      ret += this.countContainers_(childElement);
    }
  }
  return ret;
};


/**
 * Determines whether a table has only data in it (text and images),
 * not more complex HTML structure.  The presence of a non-empty
 * thead or tfoot is also a strong indicator of tabular dat.
 *
 * @param {!Element} table
 * @return {boolean}
 * @private
 */
pagespeed.MobLayout.prototype.isDataTable_ = function(table) {
  var numDataNodes = 0;

  // Tables have this hierarchy:
  // <table>
  //   <thead> <tbody> <tfoot>  (index i)
  //     <tr>                   (index j)
  //       <td>                 (index k)
  //         content            (index m) -- we don't use 'l' as an var.

  // Some tables are used for layout. Some are used for showing tabular
  // data.  If therd is a non-empty thead then we'll assume it's tabular.
  // If there is more than one row and more than one column, we'll assume
  // it's tabular as well (might be wrong about this.  We'll return 'false'
  // from this routine if it looks tabular.
  for (var tchild = table.firstChild; tchild; tchild = tchild.nextSibling) {
    for (var tr = tchild.firstChild; tr; tr = tr.nextSibling) {
      var tagName = tchild.tagName.toUpperCase();
      if ((tagName == 'THEAD') || (tagName == 'TFOOT')) {
        // The presence of a non-empty thead or tfoot is a strong signal
        // that the structure matters.
        return true;
      }
      for (var td = tr.firstChild; td; td = td.nextSibling) {
        if (td.tagName && (td.tagName.toUpperCase() == 'TH')) {
          return true;
        }
        ++numDataNodes;
      }
    }
  }

  // On some sites it looks much better to atomize the table, despite the fact
  // that the container count (23) is not too high -- the data-node count (40).
  //
  // In other sites, the numbers are much lower; the critical table has only a
  // couple of containers.  For now, many sites are happy with 3*containers as
  // the threshold, but I suspect we have not seen the last of this
  // heuristic.
  var numContainers = this.countContainers_(table);
  if ((3 * numContainers) > numDataNodes) {
    return false;
  }
  return true;
};


/**
 * Re-arrange a table so that it can possibly be resized to the
 * specified dimensions.  In quirks mode, you can't make a TD
 * behave nicely when narrowing a table, due to this code in
 * blink/webkit:
 * https://code.google.com/p/chromium/codesearch#chromium/src/third_party/WebKit/Source/core/css/resolver/StyleAdjuster.cpp&rcl=1413930987&l=310
 * See also https://bugs.webkit.org/show_bug.cgi?id=38527
 *
 * Thus we have to rip out the table and put in divs.  Note that this
 * will erase the contents of iframes anywhere in the subtrees of the table,
 * which will have to be reloaded.  This can break some iframes, and thus
 * it is preferable to use this.reorganizeTableNoQuirksMode_, which just sets
 * attributes on the table elements without changing the structure.
 *
 * @param {!Element} table
 * @param {number} maxWidth
 * @private
 */
pagespeed.MobLayout.prototype.reorganizeTableQuirksMode_ =
    function(table, maxWidth) {
  var elements = [];
  var i, j, k, m, element, data, div, new_element;

  // pagespeed.MobUtil.createXPathFromNode(table));

  // Tables have this hierarchy:
  // <table>
  //   <thead> <tbody> <tfoot>  (index i)
  //     <tr>                   (index j)
  //       <td>                 (index k)
  //         content            (index m) -- we don't use 'l' as an var.
  //
  // For now we treat rows in the head and body the same, but we
  // most certainly should not.  Probably if a table has rows in the
  // head then the structure should be changed to something else that
  // retains the visual organization of header columns to body columns.
  // E.g. one idea is turn a table with N body rows and M columns into
  // a table with 1+X columns (X small, 1-3 depending on widths), M rows,
  // and some kind of navigational element to choose which X of the original
  // rows data should be displayed.
  var replacement = document.createElement('DIV');
  replacement.style.display = 'inline-block';
  var tableChildren = this.childElements_(table);
  for (i = 0; i < tableChildren.length; ++i) {
    var bodyChildren = this.childElements_(tableChildren[i]);
    for (j = 0; j < bodyChildren.length; ++j) {
      var rowChildren = this.childElements_(bodyChildren[j]);
      for (k = 0; k < rowChildren.length; ++k) {
        data = rowChildren[k];
        // If there is more than one elment in the <td>, then
        // make a div for the elements, otherwise just
        // move the element.
        if (data.childNodes.length == 1) {
          element = data.childNodes[0];
          data.removeChild(element);
          replacement.appendChild(element);
        } else if (data.childNodes.length > 1) {
          div = document.createElement('DIV');
          div.style.display = 'inline-block';
          var dataChildren = this.childElements_(data);
          for (m = 0; m < dataChildren.length; ++m) {
            element = dataChildren[m];
            data.removeChild(element);
            div.appendChild(element);
          }
          replacement.appendChild(div);
        }
      }
    }
  }
  var parent = table.parentNode;
  parent.replaceChild(replacement, table);
};


/**
 * Re-arrange a table so that it can possibly be resized to the
 * specified dimensions.  For now, just strip out all the content
 * and make them all separate divs.
 * @param {!Element} table
 * @param {number} maxWidth
 * @private
 */
pagespeed.MobLayout.prototype.reorganizeTableNoQuirksMode_ =
    function(table, maxWidth) {
  var tnode, tchild, rnode, row, dnode, data, div;

  // Tables have this hierarchy:
  // <table>
  //   <thead> <tbody> <tfoot>  (tchild)
  //     <tr>                   (row)
  //       <td>                 (data)
  //         content
  //
  // For now we treat rows in the head and body the same, but we
  // most certainly should not.  Probably if a table has rows in the
  // head then the structure should be changed to something else that
  // retains the visual organization of header columns to body columns.
  // E.g. one idea is turn a table with N body rows and M columns into
  // a table with 1+X columns (X small, 1-3 depending on widths), M rows,
  // and some kind of navigational element to choose which X of the original
  // rows data should be displayed.
  var fullWidth = '100%';  //'' + this.maxWidth_ + 'px';
  pagespeed.MobUtil.removeProperty(table, 'width');
  pagespeed.MobUtil.setPropertyImportant(table, 'max-width', fullWidth);
  for (tnode = table.firstChild; tnode; tnode = tnode.nextSibling) {
    tchild = pagespeed.MobUtil.castElement(tnode);
    if (tchild != null) {
      pagespeed.MobUtil.removeProperty(tchild, 'width');
      pagespeed.MobUtil.setPropertyImportant(tchild, 'max-width', fullWidth);
      for (rnode = tchild.firstChild; rnode; rnode = rnode.nextSibling) {
        row = pagespeed.MobUtil.castElement(rnode);
        if ((row != null) && (row.tagName.toUpperCase() == 'TR')) {
          pagespeed.MobUtil.removeProperty(row, 'width');
          pagespeed.MobUtil.setPropertyImportant(row, 'max-width', fullWidth);
          for (dnode = row.firstChild; dnode; dnode = dnode.nextSibling) {
            data = pagespeed.MobUtil.castElement(dnode);
            if ((data != null) && (data.tagName.toUpperCase() == 'TD')) {
              pagespeed.MobUtil.setPropertyImportant(
                  data, 'max-width', fullWidth);
              pagespeed.MobUtil.setPropertyImportant(
                  data, 'display', 'inline-block');
            }
          }
        }
      }
    }
  }
};


/**
 * Override various styles on the DOM, e.g. large margins & padding,
 * percentages on left and top, etc.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.cleanupStyles_ = function(element) {
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
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.cleanupStylesHelper_ = function(element) {
  // Fixes the top bar of sites that have white-space:nowrap so that all
  // elements on the original line are visible when the width is constrained.
  // Do this before recursing into children as this property inherits, and
  // we'll need less override markup if we do it at the top level.
  var computedStyle = window.getComputedStyle(element);
  if (computedStyle.getPropertyValue('white-space') == 'nowrap') {
    pagespeed.MobUtil.setPropertyImportant(element, 'white-space', 'normal');
  }

  this.forEachMobilizableChild_(element, this.cleanupStylesHelper_);

  // After recursing into children, the computed styles on the parent
  // can change, and we need the new ones.
  computedStyle = window.getComputedStyle(element);

  var i, name, value;
  for (i = 0; i < pagespeed.MobLayout.NO_PERCENT_.length; ++i) {
    name = pagespeed.MobLayout.NO_PERCENT_[i];
    value = computedStyle.getPropertyValue(name);
    if (value && (value != '100%') && (value != 'auto') &&
        (value.length > 0) && (value[value.length - 1] == '%')) {
      pagespeed.MobUtil.setPropertyImportant(element, name, 'auto');
    }
  }

  // Don't remove the left-padding from lists; that makes the bullets
  // disappear at the bottom of some sites.  See
  //     http://www.w3schools.com/cssref/pr_list-style-position.asp
  //
  // Don't remove padding from body.
  var tagName = element.tagName.toUpperCase();
  var isList = (tagName == 'UL') || (tagName == 'OL');
  var isBody = (tagName == 'BODY');

  // Reduce excess padding on margins.  We don't want to eliminate
  // all padding as that looks terrible on many sites.
  var style = '';
  for (i = 0; i < pagespeed.MobLayout.CLAMPED_STYLES_.length; ++i) {
    name = pagespeed.MobLayout.CLAMPED_STYLES_[i];
    if ((!isList || !goog.string.endsWith(name, '-left')) &&
        (!isBody || !goog.string.startsWith(name, 'margin-'))) {
      value = pagespeed.MobUtil.computedDimension(computedStyle, name);
      if (value != null) {
        if (value > 4) {
          // Without the 'important', juniper's 'register now' field
          // has uneven input fields.
          //element.style[name] = '4px !important';
          style += name + ':4px !important;';
        } else if (value < 0) {
          if (name == 'margin-bottom') {
            // This might be a slide-show implemented with a negative
            // margin-bottom based on the element height.  However, it's
            // likely that our usage of max-width:100% and viewports has
            // caused some heights to change (without any explicit JS
            // overrides.  We then may make further adjustments to the element
            // height in expandColumns or elsewhere.  So at this
            // phase we don't adjust the margin-bottom, but just mark the
            // element with an attribute we can easily find later.
            // See http://goo.gl/gzWY6I [smashingmagazine.com]
            element.setAttribute(
                pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_, '1');
            // TODO(jmarantz): do this for margin-right as well.
          } else {
            style += name + ':0px !important;';
          }
        }
      }
    }
  }

  pagespeed.MobUtil.addStyles(element, style);
};


/**
 * Sometimes the browser resizing algorithms result in image-squeezing that's
 * too small.
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.repairDistortedImages_ = function(element) {
  this.forEachMobilizableChild_(element, this.repairDistortedImages_);
  if (element.tagName.toUpperCase() == 'IMG') {
    var computedStyle = window.getComputedStyle(element);
    var requestedWidth = pagespeed.MobUtil.findRequestedDimension(
        element, 'width');
    var requestedHeight = pagespeed.MobUtil.findRequestedDimension(
        element, 'height');
    if (requestedWidth && requestedHeight && computedStyle) {
      var width = pagespeed.MobUtil.computedDimension(computedStyle, 'width');
      var height = pagespeed.MobUtil.computedDimension(computedStyle, 'height');
      if (width && height) {
        var widthShrinkage = width / requestedWidth;
        var heightShrinkage = height / requestedHeight;
        if (!pagespeed.MobUtil.aboutEqual(widthShrinkage, heightShrinkage)) {
          console.log('aspect ratio problem for ' +
              element.getAttribute('src'));

          if (pagespeed.MobUtil.isSinglePixel(element)) {
            var shrinkage = Math.min(widthShrinkage, heightShrinkage);
            pagespeed.MobUtil.removeProperty(element, 'width');
            pagespeed.MobUtil.removeProperty(element, 'height');
            element.style.width = requestedWidth * shrinkage;
            element.style.height = requestedHeight * shrinkage;
          } else if (widthShrinkage > heightShrinkage) {
            pagespeed.MobUtil.removeProperty(element, 'height');
          } else {
            // If we let the width go free but set the height, the aspect ratio
            // might not be maintained.  A few ideas on how to fix are here
            //   http://stackoverflow.com/questions/21176336/css-image-to-have-fixed-height-max-width-and-maintain-aspect-ratio
            // Let's try changing the height attribute to max-height.
            pagespeed.MobUtil.removeProperty(element, 'width');
            pagespeed.MobUtil.removeProperty(element, 'height');
            element.style.maxHeight = requestedHeight;
          }
        }
        if (widthShrinkage < 0.25) {
          console.log('overshrinkage for ' + element.getAttribute('src'));
          this.reallocateWidthToTableData_(element);
        }
      }
    }
  }
};


/**
 * Climb up parent-nodes to find a 'td' and set the width of all the td in the
 * 'tr' to 100/X % where X is the number of td. This works for some sites
 * on Chrome.  Note that we don't get such great results in Firefox
 * responsive-design mode with a narrow screen.  Instead, the aspect ratio and
 * size of the picture is maintained, and the whole table becomes too wide.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.reallocateWidthToTableData_ = function(element) {
  var tdParent = element;
  while (tdParent && tdParent.tagName.toUpperCase() != 'TD') {
    tdParent = tdParent.parentNode;
  }
  if (tdParent) {
    var tr = tdParent.parentNode;
    if (tr) {
      var td, dnode, numTds = 0;
      for (td = tr.firstChild; td; td = td.nextSibling) {
        if (td.tagName && td.tagName.toUpperCase() == 'TD') {
          ++numTds;
        }
      }
      if (numTds > 1) {
        var style = 'width:' + Math.round(100 / numTds) + '%;';
        for (dnode = tr.firstChild; dnode; dnode = dnode.nextSibling) {
          td = pagespeed.MobUtil.castElement(dnode);
          if ((td != null) && (td.tagName.toUpperCase() == 'TD')) {
            pagespeed.MobUtil.addStyles(td, style);
          }
        }
      }
    }
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
pagespeed.MobLayout.prototype.stripFloats_ = function(element) {
  var elementStyle = window.getComputedStyle(element);
  var position = elementStyle.getPropertyValue('position');
  if (position == 'fixed') {
    return 'fixed';
  }

  // Contains nodes that we want to reorder in element, putting
  // them at the end of the child-list in reverse order to their
  // accumulation here.
  var i, child, childElement, childPosition, floatStyle, reorderNodes = [];
  var previousChild = null;
  var displayOverride, absoluteNodes = [];
  var marginBottom, previousChildHasNegativeBottomMargin = false;

  for (child = element.firstChild; child; child = child.nextSibling) {
    childElement = this.getMobilizeElement(child);
    if (childElement != null) {
      // Clean up the children first, because they might pick up 'float'
      // attributes from their parent.  If we clean the float attributes
      // from the parent first, then we won't be able to detect it when
      // testing the children.
      childPosition = this.stripFloats_(childElement);
      if ((childPosition == 'fixed') ||
          (this.getMobilizeElement(childElement) == null)) {
        // do nothing
      } else if (childPosition == 'absolute') {
        absoluteNodes.push(childElement);
      } else {
        var childStyle = window.getComputedStyle(childElement);
        floatStyle = childStyle.getPropertyValue('float');
        var floatRight = (floatStyle == 'right');
        displayOverride = 'inline-block';

        if (floatRight || (floatStyle == 'left')) {
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
              pagespeed.MobUtil.setPropertyImportant(
                  previousChild, 'margin-bottom', '0px');
            }
          }

          // It won't be effective to call style.removeProperty('float'); when
          // it's computed from CSS rules, but we can explicitly set it to
          // 'none' right on the object, which will override a value in
          // inherited from a class.
          pagespeed.MobUtil.setPropertyImportant(childElement, 'float', 'none');
          var display = childStyle.getPropertyValue('display');
          if (display != 'none') {
            // TODO(jmarantz): If we have an invisible block that's
            // got a 'float' attribute, then we don't want to make it
            // visible now; we just want to strip the 'float'.
            pagespeed.MobUtil.setPropertyImportant(
                childElement, 'display', displayOverride);
          }
        }
        if (floatRight) {
          reorderNodes.push(childElement);
        }
        previousChild = childElement;
        marginBottom = pagespeed.MobUtil.computedDimension(
            childStyle, 'margin-bottom');
        previousChildHasNegativeBottomMargin =
            ((marginBottom != null) && (marginBottom < 0));
      }
    }
  }

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
 * Once we've decided a node is being rendered as a single column, this
 * function removes any of the width constraints.
 *
 * @param {!Element} element
 * @param {Object} computedStyle
 * @private
 */
pagespeed.MobLayout.prototype.removeWidthConstraint_ =
    function(element, computedStyle) {
  // Input fields are sometimes reasonably sized, and shouldn't
  // be auto-width.
  var tagName = element.tagName.toUpperCase();
  if ((tagName != 'INPUT') && (tagName != 'SELECT')) {
    // Determine whether this element has a width constraint.
    if ((element.style.backgroundSize == '') &&
        (computedStyle.getPropertyValue('width') != 'auto')) {
      pagespeed.MobUtil.setPropertyImportant(element, 'width', 'auto');
    }
    if (tagName != 'IMG') {
      // Various table elements with explicit widths can be cleaned up
      // to let the browser decide.
      element.removeAttribute('width');
    }
    pagespeed.MobUtil.removeProperty(element, 'border-left');
    pagespeed.MobUtil.removeProperty(element, 'border-right');
    pagespeed.MobUtil.removeProperty(element, 'margin-left');
    pagespeed.MobUtil.removeProperty(element, 'margin-right');
    pagespeed.MobUtil.removeProperty(element, 'padding-left');
    pagespeed.MobUtil.removeProperty(element, 'padding-right');
    if (element.className != '') {
      element.className += ' psSingleColumn';
    } else {
      element.className = 'psSingleColumn';
    }
  }
};


/**
 * When a desktop page with multiple columns is transformed into single-column
 * mode, width-constraints can get in the way of using the available space on
 * the phone.  Thus when we are in single column mode, we should remove these
 * constraints.
 *
 * @param {!Element} element
 * @private
 */
pagespeed.MobLayout.prototype.expandColumns_ = function(element) {
  var elementStyle = window.getComputedStyle(element);
  var position = elementStyle.getPropertyValue('position');
  if (position == 'fixed') {
    return;
  }

  // Make an array of all interesting children and their computed styles.
  var next, child, childElement;
  var children = [];
  var childComputedStyles = [];
  for (child = element.firstChild; child; child = child.nextSibling) {
    childElement = this.getMobilizeElement(child);
    if (childElement != null) {
      var computedStyle = window.getComputedStyle(childElement);
      var childPosition = computedStyle.getPropertyValue('position');
      if ((childPosition == 'fixed') ||
          (childPosition == 'absolute') || (child.offsetWidth == 0)) {
        // do nothing
      } else {
        children.push(childElement);
        childComputedStyles.push(computedStyle);
      }
    }
  }

  // See if a child is positioned to the right or right of it's neighbor.  If
  // not, we can expand it and its children.
  var prevOffsetRight = null;
  for (var i = 0; i < children.length; ++i) {
    childElement = children[i];
    next = (i < children.length - 1) ? children[i + 1] : null;
    var offsetRight = childElement.offsetLeft + childElement.offsetWidth;
    if (((prevOffsetRight == null) ||
         (childElement.offsetLeft < prevOffsetRight)) &&
        ((next == null) || (next.offsetLeft < offsetRight))) {
      this.removeWidthConstraint_(childElement, childComputedStyles[i]);
      this.expandColumns_(childElement);
    }

    var attr = element.getAttribute(
        pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_);
    if (attr) {
      element.removeAttribute(pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_);
      computedStyle = window.getComputedStyle(element);
      var height = pagespeed.MobUtil.computedDimension(computedStyle, 'height');
      if (height != null) {
        pagespeed.MobUtil.setPropertyImportant(
            element, 'margin-bottom', '' + -height + 'px');
      }
    }
    prevOffsetRight = offsetRight;
  }
};


/**
 * The sequence of mobilization entry-points.  We declare this as an array
 * rather than as sequential code so that we can compute how many passes
 * there are for progress bar.
 *
 * @const
 * @private
 */
pagespeed.MobLayout.sequence_ = [
  pagespeed.MobLayout.prototype.shrinkWideElements_, 'shrink wide elements',
  pagespeed.MobLayout.prototype.stripFloats_, 'string floats',
  pagespeed.MobLayout.prototype.cleanupStyles_, 'cleanup styles',
  pagespeed.MobLayout.prototype.repairDistortedImages_,
  'repair distored images',
  pagespeed.MobLayout.prototype.resizeIfTooWide_, 'resize if too wide',
  pagespeed.MobLayout.prototype.expandColumns_, 'expand columns',
  pagespeed.MobLayout.prototype.resizeVertically_, 'resize vertically'
];
