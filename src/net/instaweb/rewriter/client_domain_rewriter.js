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
 * @fileoverview Rewrites url domains on the client side. Clicks and enter key
 * presses get intercepted. If the target url domain is a part of the mapped
 * domains, the target url domain gets rewritten.
 *
 * @author rahulbansal@google.com (Rahul Bansal)
 */

var ENTER_KEY_CODE = 13;

// Exporting functions using quoted attributes to prevent Closure Compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 * @param {string} mappedDomainNames for which the url is to be rewritten.
 */
pagespeed.ClientDomainRewriter = function(mappedDomainNames) {
  /*
   * @private
   */
  this.mappedDomainNames_ = mappedDomainNames;
};

/**
 * Listener for events on anchor tags. Since we are listening at the body,
 * checks if the event is for an anchor. Also checks that if a keypress
 * resulted in this event, it should have been due to pressing enter.
 * Processes the event only when these conditions are met.
 * @param {Object} event which triggered this listener.
 */
pagespeed.ClientDomainRewriter.prototype.anchorListener = function(event) {
  // We are interested only in ENTER on a LINK keypress events
  // IE has window.event instead of event.
  // TODO(rahulbansal): Fix the keyup, keydown handling.
  event = event || window.event;
  if (event.type == 'keypress' && event.keyCode != ENTER_KEY_CODE) {
    return;
  }

  for (var target = event.target; target != null; target = target.parentNode) {
    if (target.tagName == 'A') {
      this.processEvent(target.href, event);
      return;
    }
  }
};

/**
 * Adds anchor listeners to the body.
 */
pagespeed.ClientDomainRewriter.prototype.addEventListeners = function() {
  var me = this;
  // TODO(rahulbansal): This could potentially override existing event
  // listeners. Fix that.
  document.body.onclick = function(event) { me.anchorListener(event); };
  document.body.onkeypress = function(event) { me.anchorListener(event); };
};

/**
 * If url should be rewritten, sets window.location to the rewritten url and
 * prevents default event propagation.
 * @param {string} url for which the event is triggered.
 * @param {Object} event to be processed.
 */
pagespeed.ClientDomainRewriter.prototype.processEvent = function(url, event) {
  for (var i = 0; i < this.mappedDomainNames_.length; i++) {
    if (url.indexOf(this.mappedDomainNames_[i]) == 0) {
      window.location = window.location.protocol + '//' +
          window.location.hostname + '/' +
          url.substr(this.mappedDomainNames_[i].length);
      event.preventDefault();
      return;
    }
  }
};

/**
 * Initializes the client domain rewriter.
 * @param {string} mappedDomainNames for which the url is to be rewritten.
 */
pagespeed.clientDomainRewriterInit = function(mappedDomainNames) {
  var clientDomainRewriter = new pagespeed.ClientDomainRewriter(
      mappedDomainNames);
  pagespeed['clientDomainRewriter'] = clientDomainRewriter;
  clientDomainRewriter.addEventListeners();
};
pagespeed['clientDomainRewriterInit'] = pagespeed.clientDomainRewriterInit;
