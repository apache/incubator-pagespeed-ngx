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
goog.require('goog.dom');
goog.require('goog.dom.classlist');
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
   * Menu button in the header bar.
   * @private {Element}
   */
  this.menuButton_ = null;

  /**
   * Call button in the header bar.
   * @private {Element}
   */
  this.callButton_ = null;

  /**
   * Side nav bar.
   * @private {Element}
   */
  this.navPanel_ = null;


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
};


/**
 * Size in pixels of the header bar.
 * TODO(jud): This should be in a higher-level file, like mob.js.
 * @const
 * @private {number}
 */
pagespeed.MobNav.HEADER_BAR_HEIGHT_ = 60;


/**
 * PNG image of an arrow icon, used to indicate hierarchical menus.
 * @const
 * @private {string}
 */
pagespeed.MobNav.prototype.ARROW_ICON_ =
    'data:image/png;base64,iVBORw0KGgoA' +
    'AAANSUhEUgAAAJAAAACQCAQAAABNTyozAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA' +
    '6mAAADqYAAAXcJy6UTwAAAACYktHRAD/h4/MvwAAAAlwSFlzAAALEwAACxMBAJqcGAAAATdJ' +
    'REFUeNrt0klOBTEQREHfDI7FTEn/4M2GBULQo22wK+ICKT1lKQAAAAAAAAAAAAAAAAAA/Jm7' +
    'rmv3o+W5laU8d1uLspQYLc/SLVF8rsVoefokii9rMVqe9oni21qMlqdtovhhLUbL0y5R/LIW' +
    'o+VZylKeuuX5t4nW8tRPFBtrMV6gmolic+t9xA/VSjRonl6JBs7TI9HgeVonmiDPvkSPmfO0' +
    'SjRRnhaJJstTO9GEeWom2s7zVgZVI9HEeWokmjzPvkQPmfNcSZQkz9lEifKcSZQsz9FECfMc' +
    'SZQ0z95E23ley8S2E6XOcz3R9HmuJUqR53yiNHnOJUqV53iidHmOJUqZZ3+itHn2JXopyd3k' +
    'OZ9IntVE8qwmkmc1kTyrieRZTSTPaiJ5AAAAAAAAAAAAAAAAAGjgA62rM0XB6dNxAAAAAElF' +
    'TkSuQmCC';


/**
 * PNG image of call button. To view this image, add prefix of
 * 'data:image/gif;base64,' to it.
 * @const
 * @private {string}
 */
pagespeed.MobNav.prototype.CALL_BUTTON_ =
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
 * Return a call button image with the specified color.
 * @param {!goog.color.Rgb} color
 * @return {string}
 * @private
 */
pagespeed.MobNav.prototype.callButtonImage_ = function(color) {
  var imageTemplate = window.atob(this.CALL_BUTTON_);
  var imageData = imageTemplate.substring(0, 13) +
      String.fromCharCode(color[0], color[1], color[2]) +
      imageTemplate.substring(16, imageTemplate.length);
  var imageUrl = 'data:image/gif;base64,' + window.btoa(imageData);
  return imageUrl;
};


/**
 * Find the nav sections on the page. If site-specific tweaks are needed to add
 * to the nav sections found by the machine learning, then add them here.
 * @private
 */
pagespeed.MobNav.prototype.findNavSections_ = function() {
  var elements;
  if (window.pagespeedNavigationalIds) {
    var n = window.pagespeedNavigationalIds.length;
    elements = Array(n);
    for (var i = 0; i < n; i++) {
      var id = window.pagespeedNavigationalIds[i];
      // Attempt to use querySelector(...) if getElementById(...) fails.  This
      // handles the empty string (not retrieved by getElementById) gracefully,
      // and should deal with other corner cases as well.
      elements[i] = (document.getElementById(id) ||
                     document.querySelector(
                         '[id=' +
                         pagespeed.MobUtil.toCssString1(id) +
                         ']'));
    }
  } else {
    elements = Array(0);
  }
  this.navSections_ = elements;
};


/**
 * Do a pre-pass over all the nodes on the page to prepare them for
 * mobilization.
 * 1) Find all the elements with position:fixed, and move them down by the
 *    height of the nav bar.
 * 2) Find elements with z-index greater than the z-index we are trying to use
 *    for the nav panel, and clamp it down if it is.
 * TODO(jud): This belongs in mobilize.js instead of mobilize_nav.js.
 * @private
 */
pagespeed.MobNav.prototype.fixExistingElements_ = function() {
  var elements = document.getElementsByTagName('*');
  for (var i = 0, element; element = elements[i]; i++) {
    var style = window.getComputedStyle(element);
    if (style.getPropertyValue('position') == 'fixed') {
      var elTop = element.getBoundingClientRect().top;
      element.style.top =
          String(pagespeed.MobNav.HEADER_BAR_HEIGHT_ + elTop) + 'px';
    }

    if (style.getPropertyValue('z-index') >= 999999) {
      console.log('Element z-index exceeded 999999, setting to 999998.');
      element.style.zIndex = 999998;
    }
  }
};


/**
 * Redraw the header after scrolling or zooming finishes.
 * @private
 */
pagespeed.MobNav.prototype.redrawHeader_ = function() {
  // Redraw the bar to the top of page by offsetting by the amount scrolled.
  this.headerBar_.style.top = window.scrollY + 'px';
  this.headerBar_.style.left = window.scrollX + 'px';

  // Resize the bar by scaling to compensate for the amount zoomed.
  var scaleTransform =
      'scale(' + window.innerWidth / document.documentElement.clientWidth + ')';
  this.headerBar_.style['-webkit-transform'] = scaleTransform;
  this.headerBar_.style.transform = scaleTransform;

  // Restore visibility since the bar was hidden while scrolling and zooming.
  goog.dom.classlist.remove(this.headerBar_, 'hide');

  // Get the new size of the header bar and rescale the containing elements to
  // fit inside.
  var height = this.headerBar_.offsetHeight;

  this.menuButton_.style.width = height + 'px';
  this.menuButton_.style.height = height + 'px';

  this.callButton_.style.width = height + 'px';
  this.callButton_.style.height = height + 'px';

  this.spacerDiv_.style.height = height + 'px';

  // Use a 200ms fade in effect to make the transition smoother.
};


/**
 * Add events for capturing header bar resize.
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBarResizeEvents_ = function() {
  // Draw the header bar initially.
  this.redrawHeader_();

  // Don't redraw the header bar unless there has not been a scroll event for 50
  // ms and there are no touches currently on the screen. This keeps the
  // redrawing from happening until scrolling is finished.
  var scrollHandler = function() {
    if (this.scrollTimer_ != null) {
      window.clearTimeout(this.scrollTimer_);
      this.scrollTimer_ = null;
    }
    this.scrollTimer_ = window.setTimeout(goog.bind(function() {
      if (this.currentTouches_ == 0) {
        this.redrawHeader_();
      }
      this.scrollTimer_ = null;
    }, this), 50);
  };

  window.addEventListener('scroll', goog.bind(scrollHandler, this), false);

  // Keep track of number of touches currently on the screen so that we don't
  // redraw until scrolling and zooming is finished.
  window.addEventListener('touchstart', goog.bind(function(e) {
    this.currentTouches_ = e.targetTouches.length;
  }, this), false);

  window.addEventListener('touchmove', goog.bind(function() {
    if (!goog.dom.classlist.contains(document.body, 'noscroll')) {
      goog.dom.classlist.add(this.headerBar_, 'hide');
    }
  }, this), false);

  window.addEventListener('touchend', goog.bind(function(e) {
    this.currentTouches_ = e.targetTouches.length;
    if (this.scrollTimer_ == null && this.currentTouches_ == 0) {
      this.redrawHeader_();
    }
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
  // The header bar is position:fixed, so create an empty div at the top to move
  // the rest of the elements down.
  this.spacerDiv_ = document.createElement('div');
  document.body.insertBefore(this.spacerDiv_, document.body.childNodes[0]);
  goog.dom.classlist.add(this.spacerDiv_, 'psmob-header-spacer-div');

  this.headerBar_ = document.createElement('header');
  document.body.insertBefore(this.headerBar_, this.spacerDiv_);
  goog.dom.classlist.add(this.headerBar_, 'psmob-header-bar');
  // TODO(jud): This is defined in mob_logo.js
  this.menuButton_ = themeData.menuButton;
  this.headerBar_.appendChild(this.menuButton_);
  this.logoSpan_ = themeData.logoSpan;
  this.headerBar_.appendChild(this.logoSpan_);
  this.headerBar_.style.borderBottom =
      'thin solid ' +
      pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
  this.headerBar_.style.backgroundColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

  // Add call button.
  if (window.psAddCallButton) {
    this.callButton_ = document.createElement('button');
    goog.dom.classlist.add(this.callButton_, 'psmob-call-button');
    var callImage = document.createElement('img');
    callImage.src = this.callButtonImage_(themeData.menuFrontColor);
    this.callButton_.appendChild(callImage);
    this.headerBar_.appendChild(this.callButton_);
  }

  this.addHeaderBarResizeEvents_();
};


/**
 * Insert a style tag at the end of the head with the theme colors. The member
 * bool useDetectedThemeColor controls whether we use the color detected from
 * mob_logo.js, or a predefined color.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addThemeColor_ = function(themeData) {
  var backgroundColor = this.useDetectedThemeColor_ ?
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor) :
      '#3c78d8';
  var color = this.useDetectedThemeColor_ ?
      pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor) :
      'white';
  var css = '.psmob-header-bar { background-color: ' + backgroundColor +
            ' }\n' +
            '.psmob-nav-panel { background-color: ' + color + ' }\n' +
            '.psmob-nav-panel > ul li { color: ' + backgroundColor + ' }\n' +
            '.psmob-nav-panel > ul li a { color: ' + backgroundColor + ' }\n';
  var styleTag = document.createElement('style');
  styleTag.type = 'text/css';
  styleTag.appendChild(document.createTextNode(css));
  document.head.appendChild(styleTag);
};


/**
 * Traverse the nodes of the nav and label each A tag with its depth in the
 * hierarchy. Return an array of the A tags it labels.
 * @param {Node} node The starting navigational node.
 * @param {number} currDepth The current depth in the hierarchy used when
 *     recursing. Must be 0 on first call.
 * @return {!Array.<!Node>} The A tags labeled with depth.
 * @private
 */
pagespeed.MobNav.prototype.labelNavDepth_ = function(node, currDepth) {
  var navATags = [];
  for (var child = node.firstChild; child; child = child.nextSibling) {
    if (child.tagName == 'UL') {
      // If this is the first UL, then start labeling its nodes at depth 1.
      navATags =
          goog.array.join(navATags, this.labelNavDepth_(child, currDepth + 1));
    } else {
      if (child.tagName == 'A') {
        child.setAttribute('data-mobilize-nav-level', currDepth);
        navATags.push(child);
      }
      navATags =
          goog.array.join(navATags, this.labelNavDepth_(child, currDepth));
    }
  }
  return navATags;
};


/**
 * Traverse through the nav menu and generate an array of duplicate entries
 * (entries that have the same href and case-insensitive label. Because this is
 * recursive, it does not delete the items as it goes, instead it just populates
 * nodesToDelete. addNavPanel takes care of actually deleting the nodes.
 * @private
 */
pagespeed.MobNav.prototype.dedupNavMenuItems_ = function() {
  var aTags = document.querySelector('.psmob-nav-panel > ul a');

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
        if (aTag.parentNode.tagName == 'LI') {
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
 * @private
 */
pagespeed.MobNav.prototype.cleanupNavPanel_ = function() {
  var nodes = this.navPanel_.querySelectorAll('*');
  for (var i = 0, node; node = nodes[i]; i++) {
    node.removeAttribute('style');
    node.removeAttribute('width');
    node.removeAttribute('height');

    if (node.tagName == 'A' && node.innerText == '' &&
        node.hasAttribute('title')) {
      node.appendChild(document.createTextNode(node.getAttribute('title')));
    }
  }

  var maxImageHeight = 40;
  var images =
      this.navPanel_.querySelectorAll('img:not(.psmob-menu-expand-icon)');
  for (var i = 0, img; img = images[i]; ++i) {
    // Avoid blowing up an image over double it's natural height.
    var height = Math.min(img.naturalHeight * 2, maxImageHeight);
    img.setAttribute('height', height);
  }
};


/**
 * Add a nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addNavPanel_ = function() {
  // Create the nav panel element and insert immediatly after the header bar.
  this.navPanel_ = document.createElement('nav');
  document.body.insertBefore(this.navPanel_, this.headerBar_.nextSibling);
  goog.dom.classlist.add(this.navPanel_, 'psmob-nav-panel');
  var navTopUl = document.createElement('ul');
  this.navPanel_.appendChild(navTopUl);
  // By default, UL elements in the nav panel have display:none, which makes
  // hierarchical menus collapsed by default. However, we want the top level
  // menu to always be displayed, so give it the open class.
  goog.dom.classlist.add(navTopUl, 'open');

  for (var i = 0, nav; nav = this.navSections_[i]; i++) {
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
        var item = document.createElement('li');
        var div = item.appendChild(document.createElement('div'));
        var icon = document.createElement('img');
        div.appendChild(document.createElement('img'));
        icon.setAttribute('src', this.ARROW_ICON_);
        goog.dom.classlist.add(icon, 'psmob-menu-expand-icon');
        div.appendChild(document.createTextNode(
            navATags[j].textContent || navATags[j].innerText));
        navSubmenus[navSubmenus.length - 1].appendChild(item);
        var submenu = document.createElement('ul');
        item.appendChild(submenu);
        navSubmenus.push(submenu);
      } else {
        // Otherwise, create a new LI.
        var item = document.createElement('li');
        navSubmenus[navSubmenus.length - 1].appendChild(item);
        item.appendChild(navATags[j].cloneNode(true));
        var popCnt = navLevel1 - navLevel2;
        while ((popCnt > 0) && (navSubmenus.length > 1)) {
          navSubmenus.pop();
          popCnt--;
        }
      }
    }

    nav.parentNode.removeChild(nav);
  }

  this.dedupNavMenuItems_();
  this.cleanupNavPanel_();
};


/**
 * Event handler for clicks on the hamburger menu. Toggles the state of the nav
 * panel so that is opens/closes.
 * @private
 */
pagespeed.MobNav.prototype.toggleNavPanel_ = function() {
  goog.dom.classlist.toggle(this.headerBar_, 'open');
  goog.dom.classlist.toggle(this.navPanel_, 'open');
  goog.dom.classlist.toggle(document.body, 'noscroll');
};


/**
 * Add handlers to the hamburger button so that it expands the nav panel when
 * clicked. Also allow clicking outside of the nav menu to close the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addMenuButtonEvents_ = function() {
  document.body.addEventListener('click', function(e) {
    if (this.menuButton_.contains(/** @type {Node} */ (e.target))) {
      this.toggleNavPanel_();
      return;
    }

    // If the panel is open, allow clicks outside of the panel to close it.
    if (goog.dom.classlist.contains(this.navPanel_, 'open') &&
        !this.navPanel_.contains(/** @type {Node} */ (e.target))) {
      this.toggleNavPanel_();
      // Make sure that the only action taken because of the click is closing
      // the menu. We also make sure to set the useCapture param of
      // addEventListener to true to make sure no other DOM elements' click
      // handler is triggered.
      e.stopPropagation();
      e.preventDefault();
    }
  }.bind(this), /* useCapture */ true);
};


/**
 * Add events to the buttons in the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addNavButtonEvents_ = function() {
  var navUl = document.querySelector('nav.psmob-nav-panel > ul');
  navUl.addEventListener('click', function(e) {
    // We want to handle clicks on the LI that contains a nested UL. So if
    // somebody clicks on the expand icon in the LI, make sure we handle that
    // by popping up to the parent node.
    var target = goog.dom.isElement(e.target) &&
        goog.dom.classlist.contains(
            /** @type {Element} */ (e.target),
            'psmob-menu-expand-icon') ?
                e.target.parentNode :
                e.target;
    if (target.tagName == 'DIV') {
      // A click was registered on the div that has the hierarchical menu text
      // and icon. Open up the UL, which should be the next element.
      goog.dom.classlist.toggle(target.nextSibling, 'open');
      // Also toggle the expand icon, which will be the first child of the div.
      goog.dom.classlist.toggle(target.firstChild, 'open');
    }
  });
};


/**
 * Main entry point of nav mobilization. Should be called when logo detection is
 * finished.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobNav.prototype.Run = function(themeData) {
  // TODO(jud): Use a goog.log or the logging ability in mob.js instead of
  // console.log.
  console.log('Starting nav resynthesis.');
  this.findNavSections_();
  this.fixExistingElements_();
  this.addHeaderBar_(themeData);
  this.addThemeColor_(themeData);

  // Don't insert nav stuff if there are no navigational sections on the page or
  // if we are in an iFrame.
  // TODO(jud): If there are nav elements in the iframe, we should try to move
  // them to the top-level nav.
  if ((this.navSections_.length != 0) &&
      !pagespeed.MobUtil.inFriendlyIframe()) {
    this.addNavPanel_();
    this.addMenuButtonEvents_();
    this.addNavButtonEvents_();
  }
};
