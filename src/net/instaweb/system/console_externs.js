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
 * @fileoverview  List of all external objects/functions referenced in
 * console.js, but which are actually defined in some external file (in this
 * case in https://www.google.com/jsapi. Since we are not compiling that file
 * we need all those objects/functions to keep the same name. Listing them
 * here accomplishes that.
 *
 *
 * @author sligocki@google.com (Shawn Ligocki)
 *
 * @externs
 */


/** @type {string} */
var pagespeedStatisticsUrl = '';


/** @param {function()} callback */
google.setOnLoadCallback = function(callback) {};


/** @const */
google.visualization = {};



/**
 * @param {(string|Object)=} opt_data
 * @param {number=} opt_version
 * @constructor
 */
google.visualization.DataTable = function(opt_data, opt_version) {};


/**
 * @param {string|!Object} type
 * @param {string=} opt_label
 * @param {string=} opt_id
 * @return {number}
 */
google.visualization.DataTable.prototype.addColumn =
    function(type, opt_label, opt_id) {};


/**
 * @param {!Array=} opt_cellArray
 * @return {number}
 */
google.visualization.DataTable.prototype.addRow = function(opt_cellArray) {};


/** @return {number} */
google.visualization.DataTable.prototype.getNumberOfRows = function() {};



/** @interface */
google.visualization.IChart = function() {};


/**
 * @param {!Object} data
 * @param {Object=} opt_options
 * @return {undefined}
 */
google.visualization.IChart.prototype.draw = function(data, opt_options) {};



/**
 * @param {Node} container
 * @constructor
 * @implements {google.visualization.IChart}
 */
google.visualization.LineChart = function(container) {};


/** @override */
google.visualization.LineChart.prototype.draw = function(data, opt_options) {};
