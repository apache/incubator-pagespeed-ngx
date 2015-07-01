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
 */

goog.provide('mob.Iframe');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('pagespeed.MobUtil');



/**
 * Create the elements used for iframe mode. We use a container div and put the
 * spacer div and the iframe inside of it. This allows the use of CSS flex box
 * to automatically fit the iframe to the viewport size.
 * @constructor
 */
mob.Iframe = function() {};


/**
 * Initialize iframe mode.
 */
mob.Iframe.prototype.run = function() {
  var container = document.createElement(goog.dom.TagName.DIV);
  document.body.appendChild(container);
  container.id = pagespeed.MobUtil.ElementId.IFRAME_CONTAINER;

  var spacer = goog.dom.getRequiredElement(pagespeed.MobUtil.ElementId.SPACER);
  container.appendChild(spacer);

  var iframe = goog.dom.getRequiredElement(pagespeed.MobUtil.ElementId.IFRAME);
  container.appendChild(iframe);
};
