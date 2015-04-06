/*
 * Copyright 2013 Google Inc.
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
 * @fileoverview Code for busting ghost clicks. In PSOL, this is mainly used
 * to bust 'click' events from previous page during page navigation.
 *
 * @author ksimbili@google.com (Kishore Simbili)
 */

goog.require('wireless.events.clickbuster');

// 'preventGhostClick' takes coordinate always. The passed in coordinate is the
// click coordinate to prevent. ClickBuster prevents click event on a
// coordinate, if it is not found in any of the touch events received so far.
// But during page navigation, touch event coordinates are empty and so first
// click event at any coordinate will be considered ghost click and will be
// prevented.
// We are passing 0,0 as params, since coordinate is compulsory. It doesn't
// really matter.
wireless.events.clickbuster.preventGhostClick(0, 0);
