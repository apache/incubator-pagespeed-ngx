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
   * @private
   */
  this.useDetectedThemeColor_ = true;
};


/**
 * Size in pixels of the header bar.
 * TODO(jud): This should be in a higher-level file, like mob.js.
 * @const
 * @private
 */
pagespeed.MobNav.HEADER_BAR_HEIGHT_ = 60;


/**
 * PNG image of an arrow icon, used to indicate hierarchical menus.
 * @const
 * @private
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
 * Find the nav sections on the page. If site-specific tweaks are needed to add
 * to the nav sections found by the machine learning, then add them here.
 * @private
 */
pagespeed.MobNav.prototype.findNavSections_ = function() {
  var elements;
  if (pagespeedNavigationalIds) {
    var n = pagespeedNavigationalIds.length;
    elements = Array(n);
    for (var i = 0, id; id = pagespeedNavigationalIds[i]; i++) {
      elements[i] = document.getElementById(id);
    }
  } else {
    elements = Array(0);
  }
  this.navSections_ = goog.array.concat(
      elements,
      // TODO(jud): Fix MobilizeLabelFilter so that it identifies this section
      // as nav. This fix is for customaquariums.com.
      goog.array.toArray(document.querySelectorAll('.topNavList')));
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
  var topOffset = 0;
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
 * Insert a header bar element as the first node in the body.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBar_ = function(themeData) {
  // The header bar is position:fixed, so create an empty div at the top to move
  // the rest of the elements down.
  var spacerDiv = document.createElement('div');
  document.body.insertBefore(spacerDiv, document.body.childNodes[0]);
  goog.dom.classlist.add(spacerDiv, 'psmob-header-spacer-div');

  var header = document.createElement('header');
  document.body.insertBefore(header, spacerDiv);
  goog.dom.classlist.add(header, 'psmob-header-bar');
  // TODO(jud): This is defined in mob_logo.js
  header.innerHTML = themeData.headerBarHtml;
  header.style.borderBottom = 'thin solid ' + themeData.menuFrontColor;

  var logoSpan = document.getElementsByClassName('psmob-logo-span')[0];
  if (logoSpan) {
    header.appendChild(logoSpan);
  }
  var logo = document.querySelector('[data-mobile-role="logo"]');
  if (logo) {
    header.style.backgroundColor = logo.style.backgroundColor;
  }
};


/**
 * Insert a style tag at the end of the head with the theme colors. The member
 * bool useDetectedThemeColor controls whether we use the color detected from
 * mob_logo.js, or a predefined color.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addThemeColor_ = function(themeData) {
  var backgroundColor =
      (this.useDetectedThemeColor_ && themeData.menuBackColor) ?
      themeData.menuBackColor :
      '#3c78d8';
  var color = (this.useDetectedThemeColor_ && themeData.menuFrontColor) ?
      themeData.menuFrontColor :
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
  var nodes = document.querySelectorAll('.psmob-nav-panel *');
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
  var images = document.querySelectorAll(
      '.psmob-nav-panel img:not(.psmob-menu-expand-icon)');
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
  var header = document.getElementsByClassName('psmob-header-bar')[0];
  var navPanel = /** @type {Element} */ (document.body.insertBefore(
      document.createElement('nav'), header.nextSibling));
  goog.dom.classlist.add(navPanel, 'psmob-nav-panel');
  var navTopUl = /** @type {Element} */ (
      navPanel.appendChild(document.createElement('ul')));
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
        var icon = /** @type {Element} */ (
            div.appendChild(document.createElement('img')));
        icon.setAttribute('src', this.ARROW_ICON_);
        goog.dom.classlist.add(icon, 'psmob-menu-expand-icon');
        var text = div.appendChild(document.createTextNode(
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
  var navPanelElement = document.querySelector('.psmob-nav-panel');
  var headerBarElement = document.querySelector('.psmob-header-bar');
  goog.dom.classlist.toggle(headerBarElement, 'open');
  goog.dom.classlist.toggle(navPanelElement, 'open');
  goog.dom.classlist.toggle(document.body, 'noscroll');
};


/**
 * Add handlers to the hamburger button so that it expands the nav panel when
 * clicked. Also allow clicking outside of the nav menu to close the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addMenuButtonEvents_ = function() {
  var menuBtn = document.querySelector('.psmob-menu-button');

  document.body.addEventListener('click', function(e) {
    if (menuBtn.contains(/** @type {Node} */ (e.target))) {
      this.toggleNavPanel_();
      return;
    }

    var navPanelElement = document.querySelector('.psmob-nav-panel');
    // If the panel is open, allow clicks outside of the panel to close it.
    if (goog.dom.classlist.contains(navPanelElement, 'open') &&
        !navPanelElement.contains(/** @type {Node} */ (e.target))) {
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
