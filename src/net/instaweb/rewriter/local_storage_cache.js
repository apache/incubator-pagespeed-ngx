/*
 * Copyright 2012 Google Inc.
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
 * @fileoverview Code for caching inlined resources in local storage on first
 * view and fetching them therefrom on repeat view.
 *
 * @author matterbury@google.com (Matt Atterbury)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 */
pagespeed.LocalStorageCache = function() {
  /**
   * Flag indicating that the cookie needs to be regenerated.
   * @type {boolean}
   * @private
   */
  this.regenerate_cookie_ = true;
};

/**
 * @return {boolean} True if the given object has expired.
 * @param {*} obj is an object of the form '<expiry> <hash> <inline-data>'.
 */
pagespeed.LocalStorageCache.prototype.hasExpired = function(obj) {
  var expiry = obj.substring(0, obj.indexOf(' '));
  return (expiry != '' && expiry >= Date.now());
};

pagespeed.LocalStorageCache.prototype['hasExpired'] =
    pagespeed.LocalStorageCache.prototype.hasExpired;

/**
 * @return {string} The data part of the given object.
 * @param {*} obj is an object of the form '<expiry> <hash> <inline-data>'.
 */
pagespeed.LocalStorageCache.prototype.getData = function(obj) {
  var pos1 = obj.indexOf(' ');
  var pos2 = obj.indexOf(' ', pos1 + 1);
  return obj.substring(pos2 + 1);
};

pagespeed.LocalStorageCache.prototype['getData'] =
    pagespeed.LocalStorageCache.prototype.getData;

/**
 * Replaces the last script element in the DOM with the given element. The
 * script element was added by our C++ code, replacing the element that was
 * there, either an img or a style element. The script itself kicked off the
 * call to this method to replace itself with the original img/style element
 * using the inlined data from local storage.
 * @param {Element} newElement The element to replace with.
 */
pagespeed.LocalStorageCache.prototype.replaceLastScript = function(newElement) {
  var scripts = document.getElementsByTagName('script');
  var lastElement = scripts[scripts.length - 1];
  lastElement.parentNode.replaceChild(newElement, lastElement);
};

pagespeed.LocalStorageCache.prototype['replaceLastScript'] =
    pagespeed.LocalStorageCache.prototype.replaceLastScript;

/**
 * Inline the CSS with the given URL.
 * @param {string} url is the URL of the CSS to inline.
 */
pagespeed.LocalStorageCache.prototype.inlineCss = function(url) {
  var obj = window.localStorage.getItem(url);
  var newNode = document.createElement(obj ? 'style' : 'link');
  if (obj && !this.hasExpired(obj)) {
    newNode.type = 'text/css';
    newNode.appendChild(document.createTextNode(this.getData(obj)));
  } else {
    newNode.rel = 'stylesheet';
    newNode.href = url;
    this.regenerate_cookie_ = true;
  }
  this.replaceLastScript(newNode);
};

pagespeed.LocalStorageCache.prototype['inlineCss'] =
    pagespeed.LocalStorageCache.prototype.inlineCss;

/**
 * Inline the IMG with the given URL.
 * @param {string} url is the URL of the image to inline.
 */
pagespeed.LocalStorageCache.prototype.inlineImg = function(url) {
  var obj = window.localStorage.getItem(url);
  var newNode = document.createElement('img');
  if (obj && !this.hasExpired(obj)) {
    newNode.src = this.getData(obj);
  } else {
    newNode.src = url;
    this.regenerate_cookie_ = true;
  }
  // Copy over any other original attributes.
  for (var i = 1, n = arguments.length; i < n; ++i) {
    var pos = arguments[i].indexOf('=');
    newNode[arguments[i].substring(0, pos)] = arguments[i].substring(pos + 1);
  }
  this.replaceLastScript(newNode);
};

pagespeed.LocalStorageCache.prototype['inlineImg'] =
    pagespeed.LocalStorageCache.prototype.inlineImg;

/**
 * For each element of the given tag name, check if it has a pagespeed_lsc_hash
 * attribute and if so saves the element's hash, expiry, and data local storage.
 * If any elements are saved regenerate_cookie_ is set to true to force
 * regeneration.
 * @param {string} tagName Tag Name of elements to process.
 * @param {function ({Element})} dataFunc Function to get an element's data.
 * @private
 */
pagespeed.LocalStorageCache.prototype.processTags_ = function(tagName,
                                                              dataFunc) {
  var elements = document.getElementsByTagName(tagName);
  for (var i = 0, n = elements.length; i < n; ++i) {
    var element = elements[i];
    var hash = element.getAttribute('pagespeed_lsc_hash');
    if (hash) {
      var url = element.getAttribute('pagespeed_lsc_url');
      var expiry = element.getAttribute('pagespeed_lsc_expiry');
      var millis = (expiry ? (new Date(expiry)).getTime() : '');
      var data = dataFunc(element);
      if (!data) {
        // img.src is set to a data URI on the repeat view but is missing
        // thereafter, and we must not forget it once we have it.
        data = this.getData(window.localStorage.getItem(url));
      }
      window.localStorage.setItem('pagespeed_lsc_url:' + url,
                                  expiry + ' ' + hash + ' ' + data);
      this.regenerate_cookie_ = true;
    }
  }
};

/**
 * Save any inlined data to local storage.
 * @private
 */
pagespeed.LocalStorageCache.prototype.saveInlinedData_ = function() {
  this.processTags_('img', function(e) { return e.src; });
  this.processTags_('style',
                    function(e) {
                      return (e.firstChild ? e.firstChild.nodeValue : null);
                    });
};

/**
 * Regenerate the cookie from the hash's of unexpired objects in local storage
 * and remove all expired objects.
 * @private
 */
pagespeed.LocalStorageCache.prototype.generateCookie_ = function() {
  if (this.regenerate_cookie_) {
    var deadUns = [];
    var goodUns = [];
    var minExpiry = 0;
    var currentTime = (new Date()).getTime();
    // Process every local storage object of ours.
    for (var i = 0, n = window.localStorage.length; i < n; ++i) {
      var key = window.localStorage.key(i);
      if (key.indexOf('pagespeed_lsc_url:')) continue;  // Not one of ours.
      var obj = window.localStorage.getItem(key);
      var pos1 = obj.indexOf(' ');
      var expiry = obj.substring(0, pos1);
      if (expiry) {
        if (expiry >= currentTime) {
          deadUns.push(key);
          continue;
        } else if (expiry < minExpiry || minExpiry == 0) {
          minExpiry = expiry;
        }
      }
      var pos2 = obj.indexOf(' ', pos1 + 1);
      var hash = obj.substring(pos1 + 1, pos2);
      goodUns.push(hash);
    }
    // Set the cookie.
    var expires = '';
    if (minExpiry) expires = '; expires=' + (new Date(minExpiry)).toUTCString();
    document.cookie = '_GPSLSC=' + goodUns.join(',') + expires;
    // Remove all expired objects.
    for (var i = 0, n = deadUns.length; i < n; ++i) {
      window.localStorage.removeItem(deadUns[i]);
    }
    this.regenerate_cookie_ = false;
  }
};

/**
 * Runs the function when event is triggered.
 * @param {Window|Element} elem Element to attach handler.
 * @param {string} ev Name of the event.
 * @param {function()} func New onload handler.
 *
 * TODO(nikhilmadan): Avoid duplication with all the other JS code.
 */
pagespeed.addHandler = function(elem, ev, func) {
  if (elem.addEventListener) {
    elem.addEventListener(ev, func, false);
  } else if (elem.attachEvent) {
    elem.attachEvent('on' + ev, func);
  } else {
    var oldHandler = elem['on' + ev];
    elem['on' + ev] = function() {
      func.call(this);
      if (oldHandler) {
        oldHandler.call(this);
      }
    }
  }
};

/**
 * Initializes the local storage cache module.
 */
pagespeed.localStorageCacheInit = function() {
  // Do nothing if any required API is missing.
  if (window.localStorage) {
    var temp= new pagespeed.LocalStorageCache();
    pagespeed['localStorageCache'] = temp;
    pagespeed.addHandler(window, 'load',
                         function() {
                           temp.saveInlinedData_();
                         });
    pagespeed.addHandler(window, 'load',
                         function() {
                           temp.generateCookie_();
                         });
  }
};

pagespeed['localStorageCacheInit'] = pagespeed.localStorageCacheInit;
