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

goog.provide('mob.layoutConstants');

goog.require('goog.dom.TagName');
goog.require('goog.object');


/**
 * List of style attributes that we want to clamp to 4px max.
 * @const {!Array.<string>}
 */
mob.layoutConstants.CLAMPED_STYLES = [
  'border-bottom-width',
  'border-left-width',
  'border-right-width',
  'border-top-width',
  'left',
  'margin-bottom',
  'margin-left',
  'margin-right',
  'margin-top',
  'padding-bottom',
  'padding-left',
  'padding-right',
  'padding-top',
  'top'
];


/**
 * List of style attributes that we want to remove for single-column layouts.
 * @const {!Array.<string>}
 */
mob.layoutConstants.PROPERTIES_TO_REMOVE_FOR_SINGLE_COLUMN = [
  'border-left',
  'border-right',
  'margin-left',
  'margin-right',
  'padding-left',
  'padding-right'
];


/**
 * List of horizontal padding attributes that we want to subtract off when
 * computing the mobilization maximum width.
 * @const {!Array.<string>}
 */
mob.layoutConstants.HORIZONTAL_PADDING_PROPERTIES =
    ['padding-left', 'padding-right'];


/**
 * HTML tag names (upper-cased) that should be treated as flexible in
 * width.  This means that if their css-width is specified as being
 * too wide for our screen, we'll override it to 'auto'.
 *
 * @const {!Object.<string, boolean>}
 */
mob.layoutConstants.FLEXIBLE_WIDTH_TAGS = goog.object.createSet(
    goog.dom.TagName.A, goog.dom.TagName.DIV, goog.dom.TagName.FORM,
    goog.dom.TagName.H1, goog.dom.TagName.H2, goog.dom.TagName.H3,
    goog.dom.TagName.H4, goog.dom.TagName.P, goog.dom.TagName.SPAN,
    goog.dom.TagName.TBODY, goog.dom.TagName.TD, goog.dom.TagName.TFOOT,
    goog.dom.TagName.TH, goog.dom.TagName.THEAD, goog.dom.TagName.TR);


/**
 * List of tags which we don't touch.
 *
 * @const {!Object.<string, boolean>}
 */
mob.layoutConstants.DONT_TOUCH_TAGS = goog.object.createSet(
    goog.dom.TagName.SCRIPT, goog.dom.TagName.STYLE, goog.dom.TagName.IFRAME);


/**
 * List of attributes for which we want to remove any percentage specs.
 * @const {!Array.<string>}
 */
mob.layoutConstants.NO_PERCENT = ['left', 'width'];


/**
 * List of attributes for which we want to preserve layout even if it
 * means adding some horizontal scrolling.
 *
 * @const {!Array.<string>}
 */
mob.layoutConstants.LAYOUT_CRITICAL =
    [goog.dom.TagName.CODE, goog.dom.TagName.PRE, goog.dom.TagName.UL];
