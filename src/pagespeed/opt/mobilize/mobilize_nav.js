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
 */

// TODO(jud): Test on a wider range of browsers and restrict to only modern
// browsers. This uses a few modern web features, like css3 animations, that are
// not supported on opera mini for example.

goog.provide('pagespeed.MobNav');

goog.require('goog.array');
goog.require('goog.color');
goog.require('goog.dom');
goog.require('goog.dom.NodeType');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.json');
goog.require('goog.labs.userAgent.browser');
goog.require('goog.net.Jsonp');
goog.require('goog.string');
// goog.style adds ~400 bytes when using getSize and getTransformedSize.
goog.require('goog.style');
goog.require('pagespeed.MobTheme');
goog.require('pagespeed.MobUtil');



/**
 * Create mobile navigation menus.
 * @constructor
 */
pagespeed.MobNav = function() {
  this.navSections_ = [];

  /**
   * Controls whether we use the color detected from mob_logo.js, or a
   * predefined color.
   * @private {boolean}
   */
  this.useDetectedThemeColor_ = true;

  /**
   * The header bar element inserted at the top of the page.
   * @private {Element}
   */
  this.headerBar_ = null;

  /**
   * The style tag used to style the nav elements.
   * @private {Element}
   */
  this.styleTag_ = null;

  /**
   * Spacer div element inserted at the top of the page to push the rest of the
   * content down.
   * @private {Element}
   */
  this.spacerDiv_ = null;

  /**
   * The span containing the logo.
   * @private {Element}
   */
  this.logoSpan_ = null;

  /**
   * Menu button in the header bar. This element can be null after configuration
   * if nav is specifically disabled for a site with navDisabledForSite();
   * @private {Element}
   */
  this.menuButton_ = null;

  /**
   * Call button in the header bar.
   * @private {Element}
   */
  this.callButton_ = null;

  /**
   * Map button in the header bar.
   * @private {Element}
   */
  this.mapButton_ = null;

  /**
   * Side nav bar.
   * @private {Element}
   */
  this.navPanel_ = null;


  /**
   * Click detector div.
   * @private {Element}
   */
  this.clickDetectorDiv_ = null;


  /**
   * Tracks time since last scroll to figure out when scrolling is finished.
   * Will be null when not scrolling.
   * @private {?number}
   */
  this.scrollTimer_ = null;


  /**
   * Tracks number of current touches to know when scrolling is finished.
   * @private {number}
   */
  this.currentTouches_ = 0;


  /**
   * The y coordinate of the previous move event, used to determine the
   * direction of scrolling.
   * @private {number}
   */
  this.lastScrollY_ = 0;

  /**
   * Bool to track state of the nav bar.
   * @private {boolean}
   */
  this.isNavPanelOpen_ = false;

  /**
   * Tracks the elements which need to be adjusted after zooming to account for
   * the offset of the spacer div. This includes fixed position elements, and
   * absolute position elements rooted at the body.
   * @private {!Array<!Element>}
   */
  this.elementsToOffset_ = [];

  /**
   * Tracks if the redrawNav function has been called yet or not, to enable
   * slightly different behavior on first vs subsequent calls.
   * @private {boolean}
   */
  this.redrawNavCalled_ = false;


  /**
   * Tracks if we are on the stock android browser. Touch events are broken on
   * old version of the android browser, so we have to provide some fallback
   * functionality for them.
   * @private {boolean}
   */
  this.isAndroidBrowser_ = goog.labs.userAgent.browser.isAndroidBrowser();

  // From https://dev.opera.com/articles/opera-mini-and-javascript/
  this.isOperaMini_ = (navigator.userAgent.indexOf('Opera Mini') > -1);

  /**
   * Popup dialog for logo choices.  This is actually 2 different things:
   *   1. A table of logo choices and colors that can be clicked to change them.
   *   2. A PRE tag showing the config snippet for the chosen logo/colors.
   * @private {Element}
   */
  this.logoChoicePopup_ = null;
};


/**
 * The unscaled width of the nav panel in pixels.
 * @private @const {number}
 */
pagespeed.MobNav.NAV_PANEL_WIDTH_ = 250;


/**
 * GIF image of an arrow icon, used to indicate hierarchical menus.
 * @private @const {string}
 */
pagespeed.MobNav.ARROW_ICON_ =
    'R0lGODlhkACQAPABAP///wAAACH5BAEAAAEALAAAAACQAJAAAAL+jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8YhMKpfMpvMJjUqn1Kr1is1qt9yu9wsO' +
    'i8fksvmMTqvX7Lb7DY/L5/S6/Y7P6/f8vh8EAJATKIhFWFhziEiluBjT6AgFGdkySclkeZmS' +
    'qYnE2VnyCUokOhpSagqEmtqxytrjurnqFGtSSztLcvu0+9HLm+sbPPWbURx1XJGMPHyxLPXs' +
    'EA3dLDFNXP1wzZjNsF01/W31LH6VXG6YjZ7Vu651674VG8/l2s1mL2qXn4nHD6nn3yE+Al+5' +
    '+fcnQL6EBui1QcUwgb6IEvtRVGDporc/RhobKOooLRBIbSNLmjyJMqXKlSxbunwJM6bMmTRr' +
    '2ryJM6fOnTx7+vwJNKjQoUSLGj2KNKnSpUybOn0KVUcBADs=';


/**
 * GIF image of call button. To view this image, add prefix of
 * 'data:image/gif;base64,' to it.
 * @const {string}
 */
pagespeed.MobNav.CALL_BUTTON =
    'R0lGODlhgACAAPAAAAAAAAAAACH5BAEAAAEALAAAAACAAIAAAAL+jI+pCL0Po5y02vuaBrj7' +
    'D4bMtonmiXrkyqXuC7MsTNefLNv6nuE4D9z5fMFibEg0KkVI5PLZaTah1IlUWs0qrletl9v1' +
    'VsFcMZQMNivRZHWRnXbz4G25jY621/B1vYuf54cCyCZ4QlhoGIIYqKjC2Oh4AZkoaUEZaWmF' +
    '2acpwZnpuQAaKjpCWmbag5qqmsAa53oK6yT7SjtkO4r7o7vLS+K7Cuwg/EtsDIGcrMzLHOH8' +
    'DM0qvUlabY2JXaG9zc3ojYEYLk5IXs53Pgmovo7XfskOTyE//1lv3/yeP53Or0/nH8CAAo/B' +
    'KTjsIMJb/hYewOcwAMSF5iIamEixYcTMihY5bsRY0GNGkP9Ejtx3poUbk0GCrSR5Z8VLmDRy' +
    'qBnXMokYnEJq7WT5J8wXni86ZQF3JJYWpCkILiXKBOUYpouAGqEU1eobSCCwHvXqDmxKrmHF' +
    'PuH07drUbv3UUgHVFtVXuFuijVVLrNjbvLTm8pW79q/bu4LZ7i2M1i9isoEXQz3smObVyBqH' +
    'UlZ483Kpn5qxCOrs+TNonYZG27RkuoSo1HpXj7YFWtjlZJGlId72l9wy3bjmweI3OJ/hkFqB' +
    'O7U4KzTyuDKXaykAADs=';


/**
 * GIF image of a map button, from a google images search for
 * 'google map pin icon'
 * @const {string}
 */
pagespeed.MobNav.MAP_BUTTON =
    'R0lGODlhaQCkAPAAAAAAAAAAACH5BAEAAAEALAAAAABpAKQAAAL+jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyA9T2jQOzmve+vRP9hsQgh4hEGi3JpnIJcUqf0MX0WqwesNyhNtAN' +
    '+6rics9oTuN26jYw5o7rXnI5ve524e2rfb3vx3cS+IdCWFhyiEiiuBiCpwDJGEdDOZIXYfmo' +
    'xsQplIbR9lm2AbopdmR2GuahCkL6ARvLusr1SnuL26GbazvrOwq82zVJ/HtlwlsqHIzcalyM' +
    'dewcTT0snQh9bd081cv9LVXrfam9Df7MnKqejj2Nvmy+zh5PnyF7Dl9vX4H/7n7PXztlFEyN' +
    'kyfB00GElVyVU9jQYDeJCByFa+SwGkb+gQs38svnkeHFkOQGkSQ48aQTHir1pWzphSXMlYBm' +
    'UpFpM2bNnD/08Bzj82eOO0JvwCn6hihSGUjnHC06oykboUGgTv2JhucSrFttkvEKJaeWmV/I' +
    'joX5BYzKtGpPsm3r8S3cjXLd1o0rd66ivAbw5qXLV2+gwH33EhYs6bBhxYQOb2nsuPCeyI8n' +
    'U5ZskXFmzYIuI+YYeXNoTZ4xUyxNurRpVKoTiGrtOiPs1R8vn54NGnZu3SJx9/YNcHak4MJj' +
    'uyx+HHnJ4g3EMY948zkDmtKbJ6kOvSd2B9G3D9fpfbr28NaBki+/5jz3M+rXG23vPil88TXm' +
    'R7RfCb/+/fwN+/v/D2CAAg5IoAoFAAA7';


/**
 * PNG image of a swap icon (drawn by hand).
 * TODO(huibao): optimize this image.
 * @private @const {string}
 */
pagespeed.MobNav.SWAP_ICON_ =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIYAAABaCAAAAABY7nEZAAAABG' +
    'dBTUEAALGPC/xhBQAAAcpJREFUaN7t2ItxgkAQgOHtADvADkwH2oHpADrADlKCdoAdaAekA0' +
    'qgBEowYIZRE7jbe+xjktsCnE/m54CFm4qBxEgMIkavgwHlVQUDYHVoNTCGWZ86DYxhdudeA2' +
    'OYgExiMgIyicvwziQ6wy8TCoZHJkQM10zIGG6ZUDIcMiFmYDOhZ6AymWd8ogdwY8sEvP+j6x' +
    'gz4WMYM2FlLGfCzBgzqTUwiov81djWvXgb+bFzvFO26MEasqqVPkWXguBlLAbByDAFwcWwBM' +
    'HDsAbBwMAEQc1ABkHKwAdByHAJgorhGAQJI/8IWC5EYmRF2KolCmN/uYX+TDBjU0dYw4UyQo' +
    'JI69n/wmhKeUZ3WAMIM/rT2/34EGWc36fjXI7RlKvHU0WIcQ8CZBlTEKKMRxByjJcghBg/g5' +
    'BgzATBz5gNgpmxFAQnwxAEG8McBBPDFsTzAhY7rgxEED7jxMAFQctAB0HLqPeggTFcj+NGA2' +
    'Oso8o1MMZ7pcg0MFwyabDjeZgjM2F4tGEy4XnQWzNhe+0xZ8L4EmjKhPeVeDET9g+E+UwkPp' +
    'dmMpH5ePyVidin9GsmkouFp0yE1yxTJuJLp+9MNKzg2ipPC8nE+LuMLwqlrYBVqy8VAAAAAE' +
    'lFTkSuQmCC';


/**
 * Synthesize an image using the specified color.
 * @param {string} imageBase64
 * @param {!goog.color.Rgb} color
 * @return {?string}
 */
pagespeed.MobNav.prototype.synthesizeImage = function(imageBase64, color) {
  if (imageBase64.length <= 16) {
    return null;
  }
  var imageTemplate = window.atob(imageBase64);
  var imageData = imageTemplate.substring(0, 13) +
      String.fromCharCode(color[0], color[1], color[2]) +
      imageTemplate.substring(16, imageTemplate.length);
  var imageUrl = 'data:image/gif;base64,' + window.btoa(imageData);
  return imageUrl;
};


/**
 * Given a candidate node, see if it contains any IMG elements.
 * visibilityMatters indicates those IMGs must be visible.
 * @param {boolean} visibilityMatters
 * @param {!Node} node to inspect for images
 * @return {boolean}
 * @private
 */
pagespeed.MobNav.prototype.hasImages_ = function(visibilityMatters, node) {
  if (!goog.dom.isElement(node)) {
    return false;
  }
  if (node.nodeName.toUpperCase() === goog.dom.TagName.IMG) {
    return (!visibilityMatters || node.offsetParent !== null);
  }
  var images = node.getElementsByTagName(goog.dom.TagName.IMG);
  if (visibilityMatters) {
    // Check in reverse; see
    // http://stackoverflow.com/questions/8747086/
    for (var i = images.length - 1; i >= 0; i--) {
      if (images[i].offsetParent !== null) {
        return true;
      }
    }
    return false;
  } else {
    // Visibility doesn't matter.
    return images.length > 0;
  }
};


/**
 * Given a candidate navigational DOM element, figure out if it should be
 * widened to encompass its parent element as well.
 * @param {!Element} element
 * @return {boolean}
 * @private
 */
pagespeed.MobNav.prototype.canEnlargeNav_ = function(element) {
  var parent = element.parentNode;
  if (!parent) {
    return false;
  }
  if (parent.getElementsByTagName('SCRIPT').length > 0) {
    // Never enlarge to encompass a script.
    return false;
  }
  var elementIsVisible = element.offsetParent !== null;
  var sawElement = false;
  var preElementTextSize = 0;
  for (var child = parent.firstChild; child; child = child.nextSibling) {
    if (child == element) {
      sawElement = true;
    } else if (child.offsetParent === null && elementIsVisible) {
      // Not visible when element is visible.  Skip.  Note that element may be
      // invisible due to being in a menu (so it'll become visible on hover /
      // touch but isn't visible now); in that case we want to ignore visibility
      // of siblings when deciding whether to enlarge.
    } else if (child.nodeType != goog.dom.NodeType.TEXT &&
               child.nodeType != goog.dom.NodeType.ELEMENT) {
      // Ignore comments, CDATA, deprecated nodes, etc.
    } else if (this.hasImages_(elementIsVisible, child)) {
      // Image in parent, don't enlarge.
      return false;
    } else {
      var textSize = goog.string.trim(child.textContent).length;
      if (textSize > 0) {
        if (sawElement) {
          // Text after element, don't enlarge.
          return false;
        }
        preElementTextSize += textSize;
        if (preElementTextSize > 60) {
          // Too much pre-element text, don't enlarge.
          return false;
        }
      }
    }
  }
  return true;
};


/**
 * Find the nav sections on the page. If site-specific tweaks are needed to add
 * to the nav sections found by the machine learning, then add them here.
 * @private
 */
pagespeed.MobNav.prototype.findNavSections_ = function() {
  var elements = [];
  if (window.pagespeedNavigationalIds) {
    var n = window.pagespeedNavigationalIds.length;
    var parents = {};
    for (var i = 0; i < n; i++) {
      var id = window.pagespeedNavigationalIds[i];
      // Attempt to use querySelector(...) if getElementById(...) fails.  This
      // handles the empty string (not retrieved by getElementById) gracefully,
      // and should deal with other corner cases as well.
      var element = (document.getElementById(id) ||
                     document.querySelector(
                         '[id=' +
                         pagespeed.MobUtil.toCssString1(id) +
                         ']'));
      var parent = element.parentNode;
      // Optionally replace an element by its parent.
      // Make sure we do this at most once, and delete other children
      // once we have done so.
      if (parent) {
        // We need a key to index parents so we can detect duplicates.
        // We know that element has an id, so if the parent doesn't
        // we create one based on element.id.
        if (!parent.id) {
          // Add id to parent based on element id.
          if (element.id.match(/^PageSpeed-.*-[0-9]+$/)) {
            // Truncate numeric path if element has PageSpeed id.
            parent.id = element.id.replace(/-[0-9]+$/, '');
          } else {
            // Otherwise element has its own id; construct a parent id.
            parent.id = 'PageSpeed-' + element.id + '-P';
          }
        }
        if (!(parent.id in parents)) {
          // Not yet seen the parent.
          parents[parent.id] = this.canEnlargeNav_(element);
          elements.push(parents[parent.id] ? parent : element);
        } else if (!parents[parent.id]) {
          // Parent seen, was not enlarged.
          elements.push(element);
        } else {
          // parent is already in elements.
        }
      } else {
        elements.push(element);
      }
    }
  }
  this.navSections_ = elements;
};


/**
 * Do a pre-pass over all the nodes on the page to prepare them for
 * mobilization.
 * 1) Find and save all the elements with position:fixed so they can be updated
 *    later in redrawNav_()
 * 2) Find elements with z-index greater than the z-index we are trying to use
 *    for the nav panel, and clamp it down if it is.
 * TODO(jud): This belongs in mobilize.js instead of mobilize_nav.js.
 * @private
 */
pagespeed.MobNav.prototype.fixExistingElements_ = function() {
  var elements = document.querySelectorAll('* :not(#ps-progress-scrim)');
  for (var i = 0, element; element = elements[i]; i++) {
    var style = window.getComputedStyle(element);
    var position = style.getPropertyValue('position');
    if (position == 'fixed' ||
        (position == 'absolute' && element.parentNode == document.body)) {
      this.elementsToOffset_.push(element);
    }

    // Set to 999997 because the click detector div is set to 999998 and the
    // menu bar and nav panel are set to 999999. This function runs before those
    // elements are added, so it won't modify their z-index.
    if (style.getPropertyValue('z-index') >= 999998) {
      pagespeed.MobUtil.consoleLog(
          'Element z-index exceeded 999998, setting to 999997.');
      element.style.zIndex = 999997;
    }
  }
};


/**
 * Redraw the header after scrolling or zooming finishes.
 * @private
 */
pagespeed.MobNav.prototype.redrawHeader_ = function() {
  // Resize the bar by scaling to compensate for the amount zoomed.
  // innerWidth is the scaled size, while clientWidth does not vary with zoom
  // level.
  var scaleTransform =
      'scale(' + window.innerWidth / document.documentElement.clientWidth + ')';
  this.headerBar_.style['-webkit-transform'] = scaleTransform;
  this.headerBar_.style.transform = scaleTransform;

  // Restore visibility since the bar was hidden while scrolling and zooming.
  goog.dom.classlist.remove(this.headerBar_, 'hide');

  // Get the new size of the header bar and rescale the containing elements to
  // fit inside.
  var heightString = this.headerBar_.offsetHeight + 'px';
  if (this.menuButton_) {
    this.menuButton_.style.width = heightString;
  }
  var logoRight = 0;
  if (this.callButton_) {
    this.callButton_.style.width = heightString;
    logoRight += this.headerBar_.offsetHeight;
  }
  if (this.mapButton_) {
    this.mapButton_.style.width = heightString;
    logoRight += this.headerBar_.offsetHeight;
  }
  this.logoSpan_.style.left = heightString;
  this.logoSpan_.style.right = logoRight + 'px';

  // Update the size of the spacer div to take into account the changed relative
  // size of the header. Changing the size of the spacer div will also move the
  // rest of the content on the page. For example, when zooming in, we shrink
  // the size of the header bar, which causes the page to move up slightly. To
  // compensate, we adjust the scroll amount by the difference between the old
  // and new sizes of the spacer div.

  // Use getTransformedSize to take into account the scale transformation.
  var newHeight =
      Math.round(goog.style.getTransformedSize(this.headerBar_).height);
  var oldHeight = goog.style.getSize(this.spacerDiv_).height;
  this.spacerDiv_.style.height = newHeight + 'px';

  // Update the top offset of position: fixed elements. On the first run of this
  // function, they are offset by the size of the spacer div. On subsequent
  // runs, they are offset by the difference between the old and the new size of
  // the spacer div.
  for (var i = 0; i < this.elementsToOffset_.length; i++) {
    var el = this.elementsToOffset_[i];
    if (this.redrawNavCalled_) {
      var oldTop = el.style.top;
      oldTop = Number(el.style.top.split('px')[0]);
      el.style.top = String(oldTop + (newHeight - oldHeight)) + 'px';
    } else {
      var elTop = pagespeed.MobUtil.boundingRect(el).top;
      el.style.top = String(elTop + newHeight) + 'px';
    }
  }

  // Calling scrollBy will trigger a scroll event, but we've already updated
  // the size of everything, so set a flag that we should ignore the next
  // scroll event. Due to rounding errors from getTransformedSize returning a
  // float, newHeight and oldHeight can differ by 1 pixel, which will cause this
  // routine to get stuck firing in a loop as it tries and fails to compensate
  // for the 1 pixel difference.
  if (this.redrawNavCalled_ && newHeight != oldHeight) {
    window.scrollBy(0, newHeight - oldHeight);
  }

  // Redraw the bar to the top of page by offsetting by the amount scrolled.
  this.headerBar_.style.top = window.scrollY + 'px';
  this.headerBar_.style.left = window.scrollX + 'px';
  this.redrawNavCalled_ = true;
};


/**
 * Redraw the nav panel based on current zoom level.
 * @private
 */
pagespeed.MobNav.prototype.redrawNavPanel_ = function() {
  if (!this.navPanel_) {
    return;
  }

  var scale = window.innerWidth * .8 / pagespeed.MobNav.NAV_PANEL_WIDTH_;

  var xOffset =
      goog.dom.classlist.contains(this.navPanel_, 'open') ?
      0 : -pagespeed.MobNav.NAV_PANEL_WIDTH_ * scale;

  this.navPanel_.style.top = window.scrollY + 'px';
  this.navPanel_.style.left = window.scrollX + xOffset + 'px';

  var transform = 'scale(' + scale + ')';
  this.navPanel_.style['-webkit-transform'] = transform;
  this.navPanel_.style.transform = transform;

  var headerBarBox = this.headerBar_.getBoundingClientRect();
  this.navPanel_.style.marginTop = headerBarBox.height + 'px';

  // Opera mini does not support the css3 scale transformation nor the touch
  // events that we use heavily here. As a workaround, we don't set the height
  // here which allows the nav panel to fit the content. The user is then able
  // to pinch zoom and see the whole menu, rather than scrolling the menu div.
  if (!this.isOperaMini_) {
    this.navPanel_.style.height =
        ((window.innerHeight - headerBarBox.height) / scale) + 'px';
  }
};


/**
 * Add events for capturing header bar resize and calling the appropriate redraw
 * events after scrolling and zooming.
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBarResizeEvents_ = function() {
  // Draw the header bar initially.
  this.redrawHeader_();
  this.redrawNavPanel_();

  // Setup a 200ms delay to redraw the header and nav panel. The timer gets
  // reset upon each touchend and scroll event to ensure that the redraws happen
  // after scrolling and zooming are finished.
  var resetScrollTimer = function() {
    if (this.scrollTimer_ != null) {
      window.clearTimeout(this.scrollTimer_);
      this.scrollTimer_ = null;
    }
    this.scrollTimer_ = window.setTimeout(goog.bind(function() {
      // Stock android browser has a longstanding bug where touchend events are
      // not fired unless preventDefault is called in touchstart. However, doing
      // so prevents scrolling (which is the default behavior). Since those
      // events aren't fired, currentTouches_ does not get updated correctly. To
      // workaround, we fallback to the slightly jankier behavior of just
      // redrawing after a scroll event, even if there are potentially touches
      // still happening.
      // https://code.google.com/p/android/issues/detail?id=19827 for details on
      // the bug in question.
      if (this.isAndroidBrowser_ || this.currentTouches_ == 0) {
        this.redrawNavPanel_();
        this.redrawHeader_();
      }
      this.scrollTimer_ = null;
    }, this), 200);
  };

  // Don't redraw the header bar unless there has not been a scroll event for 50
  // ms and there are no touches currently on the screen. This keeps the
  // redrawing from happening until scrolling is finished.
  var scrollHandler = function(e) {
    if (!this.isNavPanelOpen_) {
      goog.dom.classlist.add(this.headerBar_, 'hide');
    }

    resetScrollTimer.call(this);

    if (this.navPanel_ && this.isNavPanelOpen_ &&
        !this.navPanel_.contains(/** @type {Node} */ (e.target))) {
      e.stopPropagation();
      e.preventDefault();
    }
  };

  window.addEventListener(goog.events.EventType.SCROLL,
                          goog.bind(scrollHandler, this), false);

  // Keep track of number of touches currently on the screen so that we don't
  // redraw until scrolling and zooming is finished.
  window.addEventListener(goog.events.EventType.TOUCHSTART,
                          goog.bind(function(e) {
                            this.currentTouches_ = e.touches.length;
                            this.lastScrollY_ = e.touches[0].clientY;
                          }, this), false);

  window.addEventListener(goog.events.EventType.TOUCHMOVE,
                          goog.bind(function(e) {
                            // The default android browser is unreliable about
                            // firing touch events if preventDefault is not
                            // called in touchstart (see note above). Therefore,
                            // we don't hide the nav panel here on the android
                            // browser, otherwise the bar is not redrawn if a
                            // user tries to scroll up past the top of the page,
                            // since neither a touchend event nor a scroll event
                            // fires to redraw the header.
                            if (!this.isNavPanelOpen_) {
                              if (!this.isAndroidBrowser_) {
                                goog.dom.classlist.add(this.headerBar_, 'hide');
                              }
                            } else {
                              e.preventDefault();
                            }
                          }, this), false);

  window.addEventListener(goog.events.EventType.TOUCHEND,
                          goog.bind(function(e) {
                            this.currentTouches_ = e.touches.length;
                            // Redraw the header bar if there are no more
                            // current touches.
                            if (this.currentTouches_ == 0) {
                              resetScrollTimer.call(this);
                            }
                          }, this), false);

  window.addEventListener(goog.events.EventType.ORIENTATIONCHANGE,
                          goog.bind(function() {
                            this.redrawNavPanel_();
                            this.redrawHeader_();
                          }, this), false);
};


/**
 * Insert a header bar to the top of the page. For this goal, firstly we
 * insert an empty div so all contents, except those with fixed position,
 * are pushed down. Then we insert the header bar. The header bar may contain
 * a hamburger icon, a logo image, and a call button.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBar_ = function(themeData) {
  // The header bar is position:absolute, but in C++ we create an empty div
  // at the top to move the rest of the elements down.  We need to access
  // on zooming to adjust its size.
  this.spacerDiv_ = document.getElementById('ps-spacer');
  document.body.insertBefore(this.spacerDiv_, document.body.childNodes[0]);
  goog.dom.classlist.add(this.spacerDiv_, 'psmob-header-spacer-div');

  this.headerBar_ = document.createElement(goog.dom.TagName.HEADER);
  document.body.insertBefore(this.headerBar_, this.spacerDiv_);
  goog.dom.classlist.add(this.headerBar_, 'psmob-header-bar');
  // Set the unscaled header bar height. We set it to 10% of the largest screen
  // dimension. Note that docEl.clientHeight can change slightly depending on if
  // the browser chrome is currently being shown or not, so we set the height
  // here instead of redrawHeader_() so that it doesn't change when redrawing
  // after the intial page view. Note, if you change this height, you should
  // also change MobilizeRewriteFilter::kSetSpacerHeight to match.
  // Using '10vmax' would be a lot better here, except this causes some
  // flakiness when recomputing the size to use for the buttons, and there are
  // issues on iOS 7.

  var viewportSize = goog.dom.getViewportSize();
  this.headerBar_.style.height =
      Math.round(Math.max(viewportSize.height, viewportSize.width) * .1) + 'px';
  if (!this.navDisabledForSite_()) {
    this.menuButton_ = themeData.menuButton;
    this.headerBar_.appendChild(this.menuButton_);
  }
  this.logoSpan_ = themeData.anchorOrSpan;
  this.headerBar_.appendChild(themeData.anchorOrSpan);
  this.headerBar_.style.borderBottom =
      'thin solid ' +
      pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
  this.headerBar_.style.backgroundColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

  // Add call button if a phone number is specified.
  if (window.psPhoneNumber) {
    this.addPhoneDialer_();
  }

  if (window.psMapLocation) {
    this.addMapNavigation_(themeData.menuFrontColor);
  }

  this.addHeaderBarResizeEvents_();
  this.addThemeColor_(themeData);
};


/**
 * Adds a phone dialer icon to the header bar.
 * @private
 */
pagespeed.MobNav.prototype.addPhoneDialer_ = function() {
  this.callButton_ = document.createElement(goog.dom.TagName.DIV);
  this.callButton_.id = 'psmob-phone-dialer';
  this.callButton_.href = '#';
  var phone = this.getPhoneNumber_();
  if (phone) {
    window.psPhoneNumber = phone;
    this.callButton_.onclick =
        goog.partial(pagespeed.MobUtil.trackClick, 'psmob-phone-dialer',
                     pagespeed.MobNav.dialPhone_);
  } else {
    this.callButton_.onclick =
        goog.partial(pagespeed.MobUtil.trackClick, 'psmob-phone-dialer',
                     pagespeed.MobNav.requestPhoneNumberAndCall_);
  }
  this.headerBar_.appendChild(this.callButton_);
};


/**
 * Gets a map URL based on the location.
 * @private
 * @return {string}
 */
pagespeed.MobNav.getMapUrl_ = function() {
  // TODO(jmarantz): test/fix this in iOS safari/chrome with/without
  // Google Maps installed.  Probably use 'http://maps.apple.com/?='
  //
  // I don't know the best way to do this but I asked the question on
  // Stack Overflow: http://goo.gl/0g8kEV
  //
  // Other links to explore:
  // https://developer.apple.com/library/iad/featuredarticles/
  // iPhoneURLScheme_Reference/MapLinks/MapLinks.html
  // https://developers.google.com/maps/documentation/ios/urlscheme
  // http://stackoverflow.com/questions/17915901/
  // is-there-an-android-equivalent-to-google-maps-url-scheme-for-ios
  var mapUrl = 'https://maps.google.com/maps?q=' +
      window.psMapLocation;
  return mapUrl;
};


/**
 * Loads a tracking pixel that triggers a conversion event if the conversion
 * label is set, and then navigates to a map.  Note that we navigate to the map
 * whether the conversion succeeds or fails.
 * @private
 */
pagespeed.MobNav.openMap_ = function() {
  if (!window.psMapConversionLabel) {
    // No conversion label specified; go straight to the map.
    window.location = pagespeed.MobNav.getMapUrl_();
    return;
  }

  // We will visit the map only after we get the onload/onerror for the
  // tracking pixel.
  var trackingPixel = new Image();
  trackingPixel.onload = function() {
    window.location = pagespeed.MobNav.getMapUrl_();
  };

  // The user comes first so he gets to the map even if we can't track it.
  trackingPixel.onerror = trackingPixel.onload;

  // With the handlers set up, we can load the magic pixel to track the
  // conversion.
  //
  // TODO(jmarantz): Is there a better API to report a conversion?  Should
  // we really use script=0, since this is obviously a script?  In any case
  // we should use <a ping> when available.
  trackingPixel.src = '//www.googleadservices.com/pagead/conversion/' +
      window.psConversionId +
      '/?label=' + window.psMapConversionLabel +
      '&amp;guid=ON&amp;script=0';
};


/**
 * Adds a map icon to the header bar.
 * @param {!goog.color.Rgb} color
 * @private
 */
pagespeed.MobNav.prototype.addMapNavigation_ = function(color) {
  var mapImage = document.createElement(goog.dom.TagName.IMG);
  mapImage.id = 'psmob-map-image';
  mapImage.src = this.synthesizeImage(pagespeed.MobNav.MAP_BUTTON, color);
  this.mapButton_ = document.createElement(goog.dom.TagName.A);
  this.mapButton_.id = 'psmob-map-button';
  this.mapButton_.href = '#';
  this.mapButton_.addEventListener(goog.events.EventType.CLICK, function(e) {
    e.preventDefault();
    pagespeed.MobUtil.trackClick('psmob-map-button', pagespeed.MobNav.openMap_);
  });
  this.mapButton_.appendChild(mapImage);
  this.headerBar_.appendChild(this.mapButton_);
};


/**
 * Attempts to get a static phone number, either as a debug
 * fallback or from a cookie, returning null if we don't have
 * the right phone number available.
 *
 * @private
 * @return {?string}
 */
pagespeed.MobNav.prototype.getPhoneNumber_ = function() {
  // When debugging mobilization with static references
  // to uncompiled JS files, the conversion-tracking flow does
  // not seem to work, so just dial the configured phone directly.
  //
  // Naturally if there is no configured conversion label, we can't
  // get a conversion-tracked phone-number either.
  if (window.psStaticJs || !window.psPhoneConversionLabel) {
    return window.psPhoneNumber;
  }

  // Check to see if the phone number we want was previously saved
  // in a valid cookie, with matching fallback number and conversion label.
  var match = document.cookie.match(/(^|;| )gwcm=([^;]+)/);
  if (match) {
    var gwcmCookie = match[2];
    if (gwcmCookie) {
      var cookieData = goog.json.parse(unescape(match[2]));
      if ((cookieData['fallback'] == window.psPhoneNumber) &&
          (cookieData['clabel'] == window.psPhoneConversionLabel)) {
        return cookieData['mobile_number'];
      }
    }
  }
  return null;
};


/**
 * Obtains a dynamic Google-Voice number to track conversions.  We only
 * do this when user clicks the phone icon.
 * @private
 */
pagespeed.MobNav.requestPhoneNumberAndCall_ = function() {
  if (window.psPhoneConversionLabel && window.psPhoneNumber) {
    // No request from cookie.
    var label = escape(window.psPhoneConversionLabel);
    var url = 'https://www.googleadservices.com/pagead/conversion/' +
        window.psConversionId + '/wcm?cl=' + label +
        '&fb=' + escape(window.psPhoneNumber);
    if (window.psDebugMode) {
      alert('requesting dynamic phone number: ' + url);
    }
    var req = new goog.net.Jsonp(url, 'callback');
    var errorCallback = function() {
      if (window.psDebugMode) {
        alert('error callback called');
      }
      pagespeed.MobNav.dialPhone_();
    };
    req.send(null, pagespeed.MobNav.receivePhoneNumber_, errorCallback);
  }
};


/**
 * Dials a phone number.
 * @private
 */
pagespeed.MobNav.dialPhone_ = function() {
  if (window.psPhoneNumber) {
    location.href = 'tel:' + window.psPhoneNumber;
  }
};


/**
 * Extracts a dynamic phone number from a jsonp response.  If successful, it
 * calls the phone, and saves the phone number in a cookie and also in the
 * window object.
 *
 * @private
 * @param {Object} json
 */
pagespeed.MobNav.receivePhoneNumber_ = function(json) {
  var phone = null;
  if (json && json['wcm']) {
    var wcm = json['wcm'];
    if (wcm) {
      phone = wcm['mobile_number'];
    }
  }

  if (phone) {
    // Save the phone in a cookie to reduce server requests.
    var cookieValue = {
      'expires': wcm['expires'],
      'formatted_number': wcm['formatted_number'],
      'mobile_number': phone,
      'clabel': window.psPhoneConversionLabel,
      'fallback': window.psPhoneNumber
    };
    cookieValue = goog.json.serialize(cookieValue);
    if (window.psDebugMode) {
      alert('saving phone in cookie: ' + cookieValue);
    }
    document.cookie = 'gwcm=' + escape(cookieValue) + ';path=/;max-age=' +
        (3600 * 24 * 90);

    // Save the phone number in the window object so it can be used in
    // dialPhone_().
    window.psPhoneNumber = phone;
  } else if (window.psPhoneNumber) {
    // No ad was clicked.  Call the configured phone number, which will not
    // be conversion-tracked.
    if (window.psDebugMode) {
      alert('receivePhoneNumber: ' + goog.json.serialize(json));
    }
  }
  pagespeed.MobNav.dialPhone_();
};


/**
 * Insert a style tag at the end of the head with the theme colors. The member
 * bool useDetectedThemeColor controls whether we use the color detected from
 * mob_logo.js, or a predefined color.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addThemeColor_ = function(themeData) {
  // Remove any prior style block.
  if (this.styleTag_) {
    this.styleTag_.parentNode.removeChild(this.styleTag_);
  }


  if (this.callButton_) {
    this.callButton_.style.backgroundImage = 'url(' +
        this.synthesizeImage(pagespeed.MobNav.CALL_BUTTON,
                             themeData.menuFrontColor) + ')';
  }

  var backgroundColor = this.useDetectedThemeColor_ ?
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor) :
      '#3c78d8';
  var color = this.useDetectedThemeColor_ ?
      pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor) :
      'white';
  var css =
      '.psmob-header-bar { background-color: ' + backgroundColor + '; }\n' +
      '.psmob-nav-panel { background-color: ' + color + '; }\n' +
      '.psmob-nav-panel > ul li { color: ' + backgroundColor + '; }\n' +
      '.psmob-nav-panel > ul li a { color: ' + backgroundColor + '; }\n' +
      '.psmob-nav-panel > ul li div { color: ' + backgroundColor + '; }\n';
  this.styleTag_ = document.createElement(goog.dom.TagName.STYLE);
  this.styleTag_.type = 'text/css';
  this.styleTag_.appendChild(document.createTextNode(css));
  document.head.appendChild(this.styleTag_);
};


/**
 * Traverse the nodes of the nav and label each A tag with its depth in the
 * hierarchy. Return an array of the A tags it labels.
 * @param {Node} node The starting navigational node.
 * @param {number} currDepth The current depth in the hierarchy used when
 *     recursing. Must be 0 on first call.
 * @param {boolean=} opt_inUl Track if we are currently in a UL. The labeling
 *     depth is only increased when in a nested UL.
 * @return {!Array.<!Node>} The A tags labeled with depth.
 * @private
 */
pagespeed.MobNav.prototype.labelNavDepth_ = function(node, currDepth,
                                                     opt_inUl) {
  var navATags = [];
  var inUl = opt_inUl || false;
  for (var child = node.firstChild; child; child = child.nextSibling) {
    if (child.nodeName.toUpperCase() == goog.dom.TagName.UL) {
      // If this is the first UL, then start labeling its nodes at depth 1.
      var nextDepth = inUl ? currDepth + 1 : currDepth;
      navATags = goog.array.join(navATags,
                                 this.labelNavDepth_(child, nextDepth, true));
    } else {
      if (child.nodeName.toUpperCase() == goog.dom.TagName.A) {
        child.setAttribute('data-mobilize-nav-level', currDepth);
        navATags.push(child);
      }
      navATags = goog.array.join(navATags,
                                 this.labelNavDepth_(child, currDepth, inUl));
    }
  }
  return navATags;
};


/**
 * Traverse through the nav menu and remove duplicate entries, keeping the first
 * occurance. Entries are considered duplicates if they have the same href and
 * case-insensitive label.
 * @private
 */
pagespeed.MobNav.prototype.dedupNavMenuItems_ = function() {
  var aTags = this.navPanel_.getElementsByTagName('a');

  var menuItems = {};
  var nodesToDelete = [];

  for (var i = 0, aTag; aTag = aTags[i]; i++) {
    if (!(aTag.href in menuItems)) {
      menuItems[aTag.href] = [];
      menuItems[aTag.href].push(aTag.innerHTML.toLowerCase());
    } else {
      var label = aTag.innerHTML.toLowerCase();
      if (menuItems[aTag.href].indexOf(label) == -1) {
        menuItems[aTag.href].push(label);
      } else {
        // We already have this menu item, so queue up the containing parent
        // LI tag to be removed.
        if (aTag.parentNode.nodeName.toUpperCase() == goog.dom.TagName.LI) {
          nodesToDelete.push(aTag.parentNode);
        }
      }
    }
  }

  for (var i = 0, node; node = nodesToDelete[i]; i++) {
    node.parentNode.removeChild(node);
  }
};


/**
 * Perform some cleanup on the elements in nav panel after its been created.
 * Remove style attributes from all the nodes moved into the nav menu, so that
 *     the styles set in mob_nav.css win.
 * If an A tag has no text, but has a title, use the title for the text.
 * If an A tag has no href, remove it.
 * If an IMG tag has the same src as the logo, remove it.
 * @private
 */
pagespeed.MobNav.prototype.cleanupNavPanel_ = function() {
  var nodes = this.navPanel_.querySelectorAll('*');
  var nodesToDelete = [];

  // Get the logo src so that we can remove duplicates of it that show up in the
  // menu bar.
  var logoImg = document.getElementById('psmob-logo-image');
  var logoSrc = logoImg ? logoImg.src : '';

  for (var i = 0, node; node = nodes[i]; i++) {
    node.removeAttribute('style');
    node.removeAttribute('width');
    node.removeAttribute('height');

    if (node.nodeName.toUpperCase() == goog.dom.TagName.A) {
      // We use textContent instead of innerText to see if the node has content
      // because innerText is aware of CSS styling and won't return the text of
      // hidden elements. This is problematic because this function runs while
      // the nav panel is hidden, and so innerText will return '' for these
      // elements since they are hidden.
      if (node.textContent == '' && node.hasAttribute('title')) {
        node.appendChild(document.createTextNode(node.getAttribute('title')));
      }
      if (node.href == '') {
        nodesToDelete.push(node);
      }
    } else if (node.nodeName.toUpperCase() == goog.dom.TagName.IMG) {
      if (node.src == logoSrc) {
        nodesToDelete.push(node);
      }
    }
  }

  // Now delete the marked node. We traverse the parents until we find the
  // node's enclosing LI, since this is the node we actually want to remove.
  // Note that every element added here (A and IMG tags) should be inside of an
  // LI, since this is how the menus are constructed in addNavPanel_().
  for (var i = 0, node; node = nodesToDelete[i]; i++) {
    while (node.nodeName.toUpperCase() != goog.dom.TagName.LI) {
      node = node.parentNode;
    }
    node.parentNode.removeChild(node);
  }

  var maxImageHeight = 40;
  var images =
      this.navPanel_.querySelectorAll('img:not(.psmob-menu-expand-icon)');
  for (var i = 0, img; img = images[i]; ++i) {
    // Avoid blowing up an image over double it's natural height.
    var height = Math.min(img.naturalHeight * 2, maxImageHeight);
    img.setAttribute('height', height);
  }

  // The fast click jquery plugin (https://github.com/ftlabs/fastclick) causes
  // issues when trying to scroll through the nav panel since it fires an
  // onclick immediately and the page will navigate away when the user just
  // wanted to scroll the menu. Adding this needsclick class restores the
  // default mouse event handling for that element. We add it to both a tags and
  // divs (for nested menus), since those are the elements that respond to
  // clicks in the menu.
  if (window['FastClick']) {
    var i, el, els;
    els = this.navPanel_.getElementsByTagName('a');
    for (i = 0; el = els[i]; i++) {
      goog.dom.classlist.add(el, 'needsclick');
    }
    els = this.navPanel_.getElementsByTagName('div');
    for (i = 0; el = els[i]; i++) {
      goog.dom.classlist.add(el, 'needsclick');
    }
  }

};


/**
 * Add a div for detecting clicks on the body in order to close the open nav
 * panel. This is to workaround JS that sends click events, which can be
 * difficult to differentiate from actual clicks from the user. In particular
 * https://github.com/DevinWalker/jflow/ causes this issue.
 * @private
 */
pagespeed.MobNav.prototype.addClickDetectorDiv_ = function() {
  this.clickDetectorDiv_ = document.createElement(goog.dom.TagName.DIV);
  this.clickDetectorDiv_.id = 'psmob-click-detector-div';
  document.body.insertBefore(this.clickDetectorDiv_, this.navPanel_);

  this.clickDetectorDiv_.addEventListener(
      goog.events.EventType.CLICK, goog.bind(function(e) {
        if (goog.dom.classlist.contains(this.navPanel_, 'open')) {
          this.toggleNavPanel_();
        }
      }, this), false);
};


/**
 * Add a nav panel.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addNavPanel_ = function(themeData) {
  // TODO(jud): Make sure we have tests covering the redraw flow and the events
  // called here.
  // Create the nav panel element and insert immediatly after the header bar.
  this.navPanel_ = document.createElement(goog.dom.TagName.NAV);
  document.body.insertBefore(this.navPanel_, this.headerBar_.nextSibling);
  goog.dom.classlist.add(this.navPanel_, 'psmob-nav-panel');
  var navTopUl = document.createElement(goog.dom.TagName.UL);
  this.navPanel_.appendChild(navTopUl);
  // By default, UL elements in the nav panel have display:none, which makes
  // hierarchical menus collapsed by default. However, we want the top level
  // menu to always be displayed, so give it the open class.
  goog.dom.classlist.add(navTopUl, 'open');
  var arrowIcon = this.synthesizeImage(pagespeed.MobNav.ARROW_ICON_,
                                       themeData.menuBackColor);

  for (var i = 0, nav; nav = this.navSections_[i]; i++) {
    if (nav.parentNode) {
      nav.setAttribute('data-mobilize-nav-section', i);
      var navATags = this.labelNavDepth_(nav, 0);

      var navSubmenus = [];
      navSubmenus.push(navTopUl);

      for (var j = 0, n = navATags.length; j < n; j++) {
        var navLevel1 = navATags[j].getAttribute('data-mobilize-nav-level');
        var navLevel2 = (j + 1 == n) ? navLevel1 : navATags[j + 1].getAttribute(
            'data-mobilize-nav-level');
        // Create a new submenu if the next item is nested under this one.
        if (navLevel1 < navLevel2) {
          var item = document.createElement(goog.dom.TagName.LI);
          var div =
              item.appendChild(document.createElement(goog.dom.TagName.DIV));
          if (arrowIcon) {
            var icon = document.createElement(goog.dom.TagName.IMG);
            div.appendChild(icon);
            icon.setAttribute('src', arrowIcon);
            goog.dom.classlist.add(icon, 'psmob-menu-expand-icon');
          }
          div.appendChild(document.createTextNode(
              navATags[j].textContent || navATags[j].innerText));
          navSubmenus[navSubmenus.length - 1].appendChild(item);
          var submenu = document.createElement(goog.dom.TagName.UL);
          item.appendChild(submenu);
          navSubmenus.push(submenu);
        } else {
          // Otherwise, create a new LI.
          var item = document.createElement(goog.dom.TagName.LI);
          navSubmenus[navSubmenus.length - 1].appendChild(item);
          item.appendChild(navATags[j].cloneNode(true));
          var popCnt = navLevel1 - navLevel2;
          while ((popCnt > 0) && (navSubmenus.length > 1)) {
            navSubmenus.pop();
            popCnt--;
          }
        }
      }

      if (window.psLayoutMode) {
        nav.parentNode.removeChild(nav);
      }
    }
  }

  this.dedupNavMenuItems_();
  this.cleanupNavPanel_();

  this.addClickDetectorDiv_();

  // Track touch move events just in the nav panel so that scrolling can be
  // controlled. This is to work around overflow: hidden not working as we would
  // want when zoomed in (it does not totally prevent scrolling).
  this.navPanel_.addEventListener(
      goog.events.EventType.TOUCHMOVE, goog.bind(function(e) {
        if (!this.isNavPanelOpen_) {
          return;
        }

        var currentY = e.touches[0].clientY;
        // If the event is not scrolling (pinch zoom for exaple), then prevent
        // it while the nav panel is open.
        if (e.touches.length != 1) {
          e.preventDefault();
        } else {
          // Check if we are scrolling horizontally or scrolling up past the top
          // or below the bottom. If so, stop the scroll event from happening
          // since otherwise the body behind the nav panel will also scroll.
          var scrollUp = currentY > this.lastScrollY_;
          var navPanelAtTop = (this.navPanel_.scrollTop == 0);
          // Add 1 pixel to account for rounding errors.
          var navPanelAtBottom =
              (this.navPanel_.scrollTop >=
               (this.navPanel_.scrollHeight - this.navPanel_.offsetHeight - 1));

          if (e.cancelable && ((scrollUp && navPanelAtTop) ||
                               (!scrollUp && navPanelAtBottom))) {
            e.preventDefault();
          }
          // Keep other touchmove events from happening. This function is not
          // supported on the android 2.3 stock browser.
          if (e.stopImmediatePropagation) {
            e.stopImmediatePropagation();
          }
          this.lastScrollY_ = currentY;
        }
      }, this), false);
  this.redrawNavPanel_();
};


/**
 * Event handler for clicks on the hamburger menu. Toggles the state of the nav
 * panel so that is opens/closes.
 * @private
 */
pagespeed.MobNav.prototype.toggleNavPanel_ = function() {
  pagespeed.MobUtil.trackClick(
      'psmob-menu-button-' +
      (goog.dom.classlist.contains(this.navPanel_, 'open') ? 'close' : 'open'));
  goog.dom.classlist.toggle(this.headerBar_, 'open');
  goog.dom.classlist.toggle(this.navPanel_, 'open');
  goog.dom.classlist.toggle(this.clickDetectorDiv_, 'open');
  goog.dom.classlist.toggle(document.body, 'noscroll');
  this.isNavPanelOpen_ = !this.isNavPanelOpen_;
  this.redrawNavPanel_();
};


/**
 * Add handlers to the hamburger button so that it expands the nav panel when
 * clicked. Also allow clicking outside of the nav menu to close the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addMenuButtonEvents_ = function() {
  this.menuButton_.addEventListener(
      goog.events.EventType.CLICK,
      goog.bind(function(e) { this.toggleNavPanel_(); }, this), false);
};


/**
 * Add events to the buttons in the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addNavButtonEvents_ = function() {
  var navUl = document.querySelector('nav.psmob-nav-panel > ul');
  navUl.addEventListener(goog.events.EventType.CLICK, function(e) {
    // We want to handle clicks on the LI that contains a nested UL. So if
    // somebody clicks on the expand icon in the LI, make sure we handle that
    // by popping up to the parent node.
    var target = goog.dom.isElement(e.target) &&
        goog.dom.classlist.contains(
            /** @type {Element} */ (e.target),
            'psmob-menu-expand-icon') ?
                e.target.parentNode :
                e.target;
    if (target.nodeName.toUpperCase() == goog.dom.TagName.DIV) {
      // A click was registered on the div that has the hierarchical menu text
      // and icon. Open up the UL, which should be the next element.
      goog.dom.classlist.toggle(target.nextSibling, 'open');
      // Also toggle the expand icon, which will be the first child of the div.
      goog.dom.classlist.toggle(target.firstChild, 'open');
    }
  }, false);
};


/**
 * Fallback function for quickly disabling nav on a specific site. Returns true
 * if the nav menu should not be enabled on a site.
 * TODO(jud): Add a real option to control inserting of the nav menu. That will
 * require a PSS release however.
 * @private
 * @return {boolean}
 */
pagespeed.MobNav.prototype.navDisabledForSite_ = function() {
  /*
  if (document.URL.indexOf('worldclassdriving') != -1) {
    return true;
  }
  */
  return false;
};


/**
 * Main entry point of nav mobilization. Should be called when logo detection is
 * finished.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobNav.prototype.Run = function(themeData) {
  pagespeed.MobUtil.consoleLog('Starting nav resynthesis.');
  this.findNavSections_();
  this.fixExistingElements_();
  this.addHeaderBar_(themeData);

  // Don't insert nav stuff if nav is disabled, there are no navigational
  // sections on the page or if we are in an iFrame.
  // TODO(jud): If there are nav elements in the iframe, we should try to move
  // them to the top-level nav.
  if (!this.navDisabledForSite_() && (this.navSections_.length != 0) &&
      !pagespeed.MobUtil.inFriendlyIframe()) {
    this.addNavPanel_(themeData);
    this.addMenuButtonEvents_();
    this.addNavButtonEvents_();
  }
};


/**
 * Updates header bar using the theme data.
 * @param {!Object} mobWindow
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobNav.prototype.updateHeaderBar = function(mobWindow, themeData) {
  var logoImage = themeData.logoImage();
  if (logoImage) {
    var frontColor =
        pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
    var backColor =
        pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

    var logoElement = mobWindow.document.getElementById('psmob-logo-image');
    if (logoElement) {
      logoElement.parentNode.replaceChild(themeData.logoElement, logoElement);
      //logoElement.src = logoImage;
      if (this.headerBar_) {
        this.headerBar_.style.backgroundColor = backColor;
      }
      this.logoSpan_.style.backgroundColor = backColor;
      //logoElement.style.backgroundColor = backColor;
    }

    var callButton = mobWindow.document.getElementById('psmob-phone-image');
    if (callButton) {
      callButton.src = this.synthesizeImage(pagespeed.MobNav.CALL_BUTTON,
                                            themeData.menuFrontColor);
    }

    var mapButton = mobWindow.document.getElementById('psmob-map-image');
    if (mapButton) {
      mapButton.src = this.synthesizeImage(pagespeed.MobNav.MAP_BUTTON,
                                           themeData.menuFrontColor);
    }

    var hamburgerLines =
        mobWindow.document.getElementsByClassName('psmob-hamburger-line');
    for (var i = 0, line; line = hamburgerLines[i]; ++i) {
      line.style.backgroundColor = frontColor;
    }
  }
};


// TODO(jmarantz): All the .chooser* methods should be loaded only with
// ?PageSpeedMobConfig=on, plus SWAP_ICON declared above.


/**
 * @param {!Array.<pagespeed.MobLogoCandidate>} candidates
 */
pagespeed.MobNav.prototype.chooserShowCandidates = function(candidates) {
  if (this.logoChoicePopup_) {
    this.chooserDismissLogoChoicePopup_();
    return;
  }

  var table = document.createElement(goog.dom.TagName.TABLE);
  table.className = 'psmob-logo-chooser-table';

  var thead = document.createElement(goog.dom.TagName.THEAD);
  table.appendChild(thead);
  var trow = document.createElement(goog.dom.TagName.TR);
  thead.appendChild(trow);
  function addData() {
    var td = document.createElement(goog.dom.TagName.TD);
    trow.appendChild(td);
    return td;
  }
  trow.className = 'psmob-logo-chooser-column-header';
  addData().textContent = 'Logo';
  addData().textContent = 'Foreground';
  addData().textContent = '';
  addData().textContent = 'Background';

  var tbody = document.createElement(goog.dom.TagName.TBODY);
  table.appendChild(tbody);
  for (var i = 0; i < candidates.length; ++i) {
    trow = document.createElement(goog.dom.TagName.TR);
    trow.className = 'psmob-logo-chooser-choice';
    tbody.appendChild(trow);
    var candidate = candidates[i];
    var themeData = pagespeed.MobTheme.synthesizeLogoSpan(
        candidate.logoRecord, candidate.background, candidate.foreground);
    addData().appendChild(themeData.anchorOrSpan);
    var img = themeData.logoElement;
    img.className = 'psmob-logo-chooser-image';
    img.onclick = goog.bind(this.chooserSetLogo_, this, candidate);

    var foreground = addData();
    foreground.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.foreground);
    foreground.className = 'psmob-logo-chooser-color';

    var swapTd = addData();
    swapTd.className = 'psmob-logo-chooser-color';
    var swapImg = document.createElement(goog.dom.TagName.IMG);
    swapImg.className = 'psmob-logo-chooser-swap';
    swapImg.src = pagespeed.MobNav.SWAP_ICON_;
    swapTd.appendChild(swapImg);

    var background = addData();
    background.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.background);
    background.className = 'psmob-logo-chooser-color';

    swapTd.onclick = goog.bind(this.chooserSwapColors_, this, candidate,
        foreground, background);
  }

  this.chooserDisplayPopup_(table);
};


/**
 * @param {!Element} popup
 * @private
 */
pagespeed.MobNav.prototype.chooserDisplayPopup_ = function(popup) {
  // The natural width of the table is about 350px, and we'll
  // want it to occupy 2/3 of the screen.  We'll add it to the DOM
  // hidden so we can get the width computed by the browser, and
  // thereby know how to center it.
  popup.style.visibility = 'hidden';
  document.body.appendChild(popup);

  var naturalWidth = popup.offsetWidth;
  var fractionOfScreen = 2.0 / 3.0;
  var scale = window.innerWidth * fractionOfScreen / naturalWidth;
  var offset = Math.round(0.5 * (1 - fractionOfScreen) * window.innerWidth) +
      'px';

  var transform =
      'scale(' + scale + ')' +
      ' translate(' + offset + ',' + offset + ')';
  popup.style['-webkit-transform'] = transform;
  popup.style.transform = transform;

  // Now that we have transformed it, make it show up.
  popup.style.visibility = 'visible';

  if (this.logoChoicePopup_ != null) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
  }
  this.logoChoicePopup_ = popup;
};


/**
 * Sets the logo in response to clicking on an image in the logo chooser
 * popup.
 * @param {!pagespeed.MobLogoCandidate} candidate
 * @private
 */
pagespeed.MobNav.prototype.chooserSetLogo_ = function(candidate) {
  var themeData = pagespeed.MobTheme.createThemeData(
      candidate.logoRecord, candidate.background, candidate.foreground);
  pagespeed.MobTheme.installLogo(themeData);
  this.updateHeaderBar(window, themeData);
  this.addThemeColor_(themeData);

  var configSnippet = document.createElement(goog.dom.TagName.PRE);
  configSnippet.className = 'psmob-logo-chooser-config-fragment';

  // TODO(jmarantz): Generate nginx syntax as needed.
  configSnippet.textContent =
      'ModPagespeedMobTheme "\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuBackColor) + '\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuFrontColor) + '\n' +
      '    ' + themeData.logoElement.src + '"';
  this.chooserDisplayPopup_(configSnippet);

  // TODO(jmarantz): consider adding a note to this popup that about how
  // you can touch the logo to bring it up the chooser again.
};


/**
 * Swaps the background and colors for a logo candidate.
 * @param {!pagespeed.MobLogoCandidate} candidate
 * @param {!Element} foregroundTd table data element (TD) for the foreground
 * @param {!Element} backgroundTd table data element (TD) for the background
 * @private
 */
pagespeed.MobNav.prototype.chooserSwapColors_ = function(
    candidate, foregroundTd, backgroundTd) {
  // TODO(jmarantz): we probably only want to swap the fg/bg for the menus,
  // and not for the header bar.  The logo background computation is generally
  // correct, as far as I can tell, and it's only a question of whether the
  // menus would look better in reverse video.
  var tmp = candidate.background;
  candidate.background = candidate.foreground;
  candidate.foreground = tmp;
  tmp = foregroundTd.style['background-color'];
  foregroundTd.style['background-color'] =
      backgroundTd.style['background-color'];
  backgroundTd.style['background-color'] = tmp;
};


/**
 * Dismisses any logo-choice pop.
 * @private
 */
pagespeed.MobNav.prototype.chooserDismissLogoChoicePopup_ = function() {
  if (this.logoChoicePopup_) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
    this.logoChoicePopup_ = null;
  }
};
