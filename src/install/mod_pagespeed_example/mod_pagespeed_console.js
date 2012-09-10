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
 * @fileoverview Code for running the mod_pagespeed console.
 * This code fetches data from the server and renders it as a time series line
 * graph or as a histogram, depending on the type of data. This data includes
 * rates of various items (rewrites, cache hits, etc.) as well as aggregate
 * statistics (the histograms).
 *
 * @author sarahdw@google.com (Sarah Dapul-Weberman)
 * @author bvb@google.com (Ben VanBerkum)
 *
 */


'use strict';

goog.require('goog.debug.ErrorHandler');
goog.require('goog.events');
goog.require('goog.object');
goog.require('goog.positioning.Corner');
goog.require('goog.string');
goog.require('goog.ui.Dialog');
goog.require('goog.ui.MenuItem');
goog.require('goog.ui.PopupDatePicker');
goog.require('goog.ui.PopupMenu');

goog.provide('pagespeed.MpsConsole');

/**
 * @constructor
 */
pagespeed.MpsConsole = function() {
  /**
   * The different types of Line Graph Types.
   * @enum {string}
   * @private
   */
  this.LineGraphTypes_ = {
    // Represents a variable over the sum of itself and another variable.
    RATIO_OVER_SUM: 'ratio_over_sum',
    // Represents the ratio of a variable over another variable.
    RATIO: 'ratio',
    // Represents the ratio of a variable over the elapsed time.
    RATE: 'rate',
    // Represents the value of a variable at that point in time.
    RAW_VALUE: 'raw_value',
    // Represents a graph with several variables graphed over elapsed time. Also
    // graphs the sum of all the values.
    MULTIPLE_RAW_VALUES: 'multiple_raw_values',
    // Represents a graph with the ratios of several variables to elapsed time.
    // Also graphs the sum of all the values.
    MULTIPLE_RATE: 'multiple_rate'
  };

  /**
   * A list of the different line graph titles, the variables that
   * the line graph data consists of, and the type of graph, which determines
   * how the data points should be calculated from the given variables.
   * Any variables added here must also be added to kImportant in
   * shared_mem_statistics.cc.
   * @type {Array.<Object.<string, string, Array.<string> > >}
   * @private
   */
  this.LineGraphs_ = [
    {title: 'Percent Cache Hits', type: this.LineGraphTypes_.RATIO_OVER_SUM,
     variables: ['cache_hits', 'cache_misses']},
    {title: 'Serf Bytes Fetched per Request', type: this.LineGraphTypes_.RATIO,
     variables: ['serf_fetch_request_count', 'serf_fetch_bytes_count']},
    {title: 'Average Load Time (ms)', type: this.LineGraphTypes_.RATIO,
     variables: ['total_page_load_ms', 'page_load_count']},
    {title: 'Queries per Second', type: this.LineGraphTypes_.RATE,
     variables: ['page_load_count']},
    {title: 'Flushes per Page', type: this.LineGraphTypes_.RATIO,
     variables: ['num_flushes', 'page_load_count']},
    {title: 'Fallback Responses Served per Second',
     type: this.LineGraphTypes_.RATE,
     variables: ['num_fallback_responses_served']},
    {title: 'Rewrites Executed per Second', type: this.LineGraphTypes_.RATE,
     variables: ['num_rewrites_executed']},
    {title: 'Rewrites Dropped per Second', type: this.LineGraphTypes_.RATE,
     variables: ['num_rewrites_dropped']},
    {title: 'Percent Rewrites Succeeded',
     type: this.LineGraphTypes_.RATIO_OVER_SUM,
     variables: ['num_rewrites_executed', 'num_rewrites_dropped']},
    {title: 'Resource 404s per Second', type: this.LineGraphTypes_.RATE,
     variables: ['resource_404_count']},
    {title: 'Slurp 404s per Second', type: this.LineGraphTypes_.RATE,
     variables: ['slurp_404_count']},
    {title: 'Ongoing Image Rewrites', type: this.LineGraphTypes_.RAW_VALUE,
     variables: ['image_ongoing_rewrites']},
    {title: 'Total Bytes Saved', type: this.LineGraphTypes_.MULTIPLE_RAW_VALUES,
     variables: ['javascript_total_bytes_saved', 'css_filter_total_bytes_saved',
                 'image_rewrite_total_bytes_saved'],
     titles: ['JavaScript', 'CSS', 'Images', 'Total']},
    {title: 'Percent Memcached Hits', type: this.LineGraphTypes_.RATIO_OVER_SUM,
     variables: ['memcached_hits', 'memcached_misses']},
    {title: 'Dropped Image Rewrites per Second',
     type: this.LineGraphTypes_.MULTIPLE_RATE,
     variables: ['image_norewrites_high_resolution',
                 'image_rewrites_dropped_due_to_load',
                 'image_rewrites_dropped_intentionally'],
     titles: ['High resolution', 'Load', 'Intentional', 'Total']},
    {title: 'CSS Flatten Imports Errors per Second',
     type: this.LineGraphTypes_.MULTIPLE_RATE,
     variables: ['flatten_imports_charset_mismatch',
                 'flatten_imports_invalid_url',
                 'flatten_imports_limit_exceeded',
                 'flatten_imports_minify_failed',
                 'flatten_imports_recursion'],
     titles: ['Character set mismatch', 'Invalid URL', 'Limit exceeded',
              'Minify failed', 'Recursion', 'Total']},
    {title: 'CSS Parse Errors per Second', type: this.LineGraphTypes_.RATE,
     variables: ['css_filter_parse_failures']},
    {title: 'JS Minification Errors per Second',
     type: this.LineGraphTypes_.RATE,
     variables: ['javascript_minification_failures']},
    {title: 'Meta Tags Converted per Second', type: this.LineGraphTypes_.RATE,
     variables: ['converted_meta_tags']}
  ];

  /**
   * A list of the different histogram titles and the variable the data is
   * mapped to.
   * @type {Array.<Object.<string, string> >}
   * @private
   */
  this.Histograms_ = [
    {title: 'Html Time (microseconds)', variable: 'Html Time us Histogram'},
    {title: 'Rewrite Latency', variable: 'Rewrite Latency Histogram'},
    {title: 'Pagespeed Resource Latency',
     variable: 'Pagespeed Resource Latency Histogram'},
    {title: 'Backend Fetch First Byte Latency',
     variable: 'Backend Fetch First Byte Latency Histogram'},
    {title: 'Memcached Get Count', variable: 'memcached_get_count'},
    {title: 'Memcached Hit Latency', variable: 'memcached_hit_latency_us'},
    {title: 'Memcached Insert Latency',
     variable: 'memcached_insert_latency_us'},
    {title: 'Memcached Insert Size', variable: 'memcached_insert_size_bytes'},
    {title: 'Memcached Lookup Size', variable: 'memcached_lookup_size_bytes'}
  ];

  /**
   * The graph currently being edited.
   * @type {number}
   * @private
   */
  this.currentlyEditing_ = 0;

  /**
   * The data tables behind the charts (from the Charts API).
   * @type {Array.<Object>}
   * @private
   */
  this.dataTables_ = [];

  /**
   * The Charts API objects representing each of the graphs.
   * @type {Array.<Object>}
   * @private
   */
  this.graphs_ = [];

  /**
   * The metrics that each graph is measuring.
   * This is typed to * to allow JSON storage and retrieval.
   * @type {*}
   * @private
   */
  this.graphMetrics_ = [];

  /**
   * Whether the given graph is a histogram.
   * This is typed to * to allow JSON storage and retrieval.
   * @type {*}
   * @private
   */
  this.histograms_ = [];

  /**
   * The interval that's responsible for updating graphs.
   * @type {?number}
   * @private
   */
  this.updateInterval_ = null;

  /**
   * Whether we've paused auto-updating.
   * @type {boolean}
   * @private
   */
  this.updatePaused_ = false;

  /**
   * The edit dialog window.
   * @type {goog.ui.Dialog}
   * @private
   */
  this.editDialog_ = null;

  /**
   * Time between updates if updateInterval is set.
   * @type {number}
   * @const
   */
  this.UPDATE_INTERVAL_MS = 7000;

  /**
   * Time window shown if updateInterval is set.
   * @type {number}
   * @const
   */
  this.TIME_WINDOW_MS = 120000; // 2 minutes

  /**
   * The options used for drawing line graphs.
   * @type {Object}
   * @private
   * @const
   */
  this.lineGraphOptions_ = {
    'width': 900,
    'height': 255,
    'colors': ['#4ECDC4', '#556270', '#C7F464'],
    'legend': {
      'position': 'bottom'
    },
    'hAxis': {
      'format': 'MMM d, y hh:mma',
      'gridlines': {
        'color': '#F2F2F2'
      },
      'baseline': {
        'color': '#E5E5E5'
      }
    },
    'vAxis': {
      'minValue': 0,
      'viewWindowMode': 'explicit',
      'viewWindow': {
        'min': 0
      },
      'gridlines': {
        'color': '#F2F2F2'
      },
      'baseline': {
        'color': '#E5E5E5'
      }
    },
    'chartArea': {
      'left': 60,
      'top': 20,
      'width': 800
    },
    'pointSize': 2
  };

  /**
   * The options used for drawing bar graphs.
   * @type{Object}
   * @private
   * @const
   */
  this.barGraphOptions_ = {
    'width': 900,
    'colors': ['#556270'],
    'legend': {position: 'none'},
    'hAxis': {
      'gridlines': {
        'color': '#F2F2F2'
      }
    },
    'vAxis': {
      'textStyle': {
        'fontSize': 8
      }
    },
    'chartArea': {
      'left': 200,
      'top': 10,
      'width': 900
    }
  };
};

/**
 * Runs the console.
 * @return {pagespeed.MpsConsole} The console object.
 */
pagespeed.initConsole = function() {
  var mpsconsole = new pagespeed.MpsConsole();
  mpsconsole.createDivs();
  var isHistogram = true;
  var endDate = new Date().getTime();
  if (!mpsconsole.loadGraphs()) {
    // Set some decent defaults if loading a saved configuration failed.
    // Flushes per Page
    mpsconsole.getGraph(mpsconsole.LineGraphs_[4].title, [4], !isHistogram, 0,
                        endDate, 5000);
    // HTML time histogram
    mpsconsole.getGraph(mpsconsole.Histograms_[0].title, [0], isHistogram, 0,
                        endDate, 5000);
    // Fallback Responses Served per Second and Rewrites Executed per Second
    mpsconsole.getGraph(mpsconsole.LineGraphs_[5].title + ' and ' +
                        mpsconsole.LineGraphs_[6].title, [5, 6], !isHistogram,
                        0, endDate, 5000);
    // Total Bytes Saved
    mpsconsole.getGraph(mpsconsole.LineGraphs_[11].title, [11], !isHistogram, 0,
                        endDate, 5000);
  }
  window.onunload = function() {
    mpsconsole.saveGraphs();
  };
  return mpsconsole;
};

/**
 * Object that holds the information about the queried variables.
 * @typedef {Object.<string, (number|Date)>}
 */
pagespeed.MpsConsole.varData;

/**
 * Array that holds the list of the timestamps queried.
 * @typedef {Array.<number>}
 */
pagespeed.MpsConsole.timestampData;

/**
 * Object that holds the information about the queried histograms.
 * @typedef {Object.<string, (Array.<Object.<string, number>>)>}
 */
pagespeed.MpsConsole.histogramData;

/**
 * Object that holds the response received from the server, containing the
 * variable data and timestamp data or the histogram data.
 * @typedef {Object.<string, (Array.<pagespeed.MpsConsole.varData|
                                      pagespeed.MpsConsole.histogramData|
                                      pagespeed.MpsConsole.timestampData>)>} */
pagespeed.MpsConsole.JSONData;

/**
 * createDivs creates the layout of the console page.
 */
pagespeed.MpsConsole.prototype.createDivs = function() {
  var toggleMessages = document.getElementById('toggle-messages');
  toggleMessages.onclick = this.loadMessagesData;
  this.createAddWidgetDialog();
  this.createEditWidgetDialog();
  var toggleAutoUpdateButton = document.getElementById('auto-update');
  var mpsconsole = this;
  toggleAutoUpdateButton.onclick = function() {
    mpsconsole.toggleAutoUpdate();
  };
};

/**
 * createAddWidgetDialog adds functionality to the add Widget modal dialog
 * by calling various helper functions. Uses the closure library for modal
 * dialog implementation.
 */
pagespeed.MpsConsole.prototype.createAddWidgetDialog = function() {
  var dialog1 = new goog.ui.Dialog();
  var addWidgetDialog = document.getElementById('add-widget');
  this.createGraphList('add-graph-list');
  this.createCompareList('add-graph-compare-list');
  dialog1.setContent(addWidgetDialog.innerHTML);
  dialog1.setTitle('Add a new Widget');
  addWidgetDialog.parentNode.removeChild(addWidgetDialog);
  var mpsconsole = this;
  goog.events.listen(dialog1, goog.ui.Dialog.EventType.SELECT, function(e) {
    // e.key is the name of the button pressed - in this case, ok or cancel.
    if (e.key == 'ok') {
      mpsconsole.addGraph();
    }
    mpsconsole.resetAddWidget();
  });
  // Causes modal popup to display when 'Add Graph' button is clicked.
  document.getElementById('modal-launcher').onclick = function() {
    if (mpsconsole.updateInterval_) {
      mpsconsole.stopAutoUpdate();
      mpsconsole.updatePaused_ = true;
      // We'll start the updating again after the graph is added.
    }
    dialog1.setVisible(true);
    mpsconsole.setupAddWidget();
  };
};

/**
 * createGraphList adds functionality to the dropdown list of possible graphs
 * to show and the associated default title.
 * @param {string} graphListId The id of the graph list.
 */
pagespeed.MpsConsole.prototype.createGraphList = function(graphListId) {
  var graphList = document.getElementById(graphListId);
  this.addMenuOption(graphListId, 'Add a Metric', -1);
  for (var i = 0; i < this.LineGraphs_.length; i++) {
    this.addMenuOption(graphListId, this.LineGraphs_[i].title, i);
  }
  for (var i = 0; i < this.Histograms_.length; i++) {
    this.addMenuOption(graphListId, this.Histograms_[i].title, i);
  }
};

/**
 * createEditWidgetDialog adds the functionality to the edit widget dialog that
 * appears when an edit graph button is pressed.
 */
pagespeed.MpsConsole.prototype.createEditWidgetDialog = function() {
  var dialog1 = new goog.ui.Dialog();
  var editWidgetDialog = document.getElementById('edit-widget');
  this.createCompareList('edit-graph-list');
  this.createCompareList('edit-graph-compare-list');
  dialog1.setContent(editWidgetDialog.innerHTML);
  dialog1.setTitle('Edit Widget');
  editWidgetDialog.parentNode.removeChild(editWidgetDialog);
  var mpsconsole = this;
  goog.events.listen(dialog1, goog.ui.Dialog.EventType.SELECT, function(e) {
    if (e.key == 'ok') {
      var graphList2 = document.getElementById('edit-graph-list');
      var metrics = [parseInt(
          graphList2.options[graphList2.selectedIndex].value, 10)];
      if (metrics[0] < 0) {
        // They picked nothing.
        return;
      }
      var graphList4 = document.getElementById('edit-graph-compare-list');
      metrics.push(parseInt(
          graphList4.options[graphList4.selectedIndex].value, 10));
      if (metrics[1] < 0) {
        metrics.splice(1);
      }
      var startTime = document.getElementById('start-date-edit').innerHTML;
      var endTime = document.getElementById('end-date-edit').innerHTML;
      if (!startTime) {
        startTime = new Date(0).getTime();
      } else {
        startTime = new Date(startTime).getTime();
      }
      if (!endTime) {
        endTime = new Date().getTime();
      } else {
        endTime = new Date(endTime).getTime();
      }
      if (startTime == endTime) {
        var num_ms_in_day = 86400000;
        endTime = endTime + num_ms_in_day;
      }
      var granularity_ms =
          document.getElementById('granularity-edit').value *= 1000;
      // Default value: 5 seconds.
      if (granularity_ms == '' || granularity_ms == 0) granularity_ms = 5000;

      var title = document.getElementById('widget-title-edit').value;

      mpsconsole.redrawLineGraph(mpsconsole.currentlyEditing_, title, metrics,
                                 startTime, endTime, granularity_ms, false);
    }
    mpsconsole.resetEditWidget();
  });
  this.editDialog_ = dialog1;
};

/**
 * redrawLineGraph takes an existing graph and redraws it, possibly adding new
 * datasets to it or changing its parameters.
 * @param {number} graphNumber The index corresponding to the line graph.
 * @param {string} newGraphTitle The new display title of the line graph.
 * @param {Array.<number>} graphsToShow A list of the line graphs to display
 *     on the same axes.
 * @param {number} startTime The start time of the data queried.
 * @param {number} endTime The end time of the data queried.
 * @param {number} granularityMs The minimum difference in time between data
 *   points queried.
 * @param {boolean} isUpdate Whether this was called as part of an update cycle.
 */
pagespeed.MpsConsole.prototype.redrawLineGraph =
    function(graphNumber, newGraphTitle, graphsToShow, startTime, endTime,
             granularityMs, isUpdate) {
  var xhr = new XMLHttpRequest();
  var mpsconsole = this;
  var queryString = this.createQueryURI(false, graphsToShow, startTime, endTime,
                                        granularityMs);
  xhr.onreadystatechange = function() {
    if (this.readyState != 4) {
      return;
    }
    if (this.status != 200 || this.responseText[0] != '{') {
      document.getElementById('mod-error').style.display = 'block';
      return;
    }
    var data = JSON.parse(this.responseText);
    document.getElementById('mod-error').style.display = 'none';
    if (isUpdate && !mpsconsole.arraysEqual(
        mpsconsole.graphMetrics_[graphNumber], graphsToShow)) {
      // We've edited the graph since starting the update, so stop now to avoid
      // losing the edit changes.
      return;
    }
    var timeSeriesData = [];
    for (var i = 0; i < graphsToShow.length; ++i) {
      if (mpsconsole.LineGraphs_[graphsToShow[i]].type ==
          mpsconsole.LineGraphTypes_.MULTIPLE_RAW_VALUES ||
          mpsconsole.LineGraphs_[graphsToShow[i]].type ==
          mpsconsole.LineGraphTypes_.MULTIPLE_RATE) {
        timeSeriesData = timeSeriesData.concat(
            mpsconsole.computeMultipleTimeSeries(data['variables'],
                                                 data['timestamps'],
                                                 graphsToShow[i]));
      } else {
        timeSeriesData.push(mpsconsole.computeTimeSeries(data['variables'],
            data['timestamps'], graphsToShow[i]));
      }
    }
    mpsconsole.updateLineGraph(timeSeriesData, data['timestamps'], graphsToShow,
                               newGraphTitle, graphNumber);
    var graphTitle = document.getElementById('mod-title' + graphNumber);
    graphTitle.innerHTML = newGraphTitle;
  };

  xhr.open('GET', queryString);
  xhr.send();
};

/**
 * Return whether the two arrays contain the same elements.
 * @return {boolean} Whether the two arrays are the same.
 */
pagespeed.MpsConsole.prototype.arraysEqual = function(a, b) {
  if (a.length != b.length) {
    return false;
  }
  for (var i = 0; i < a.length; ++i) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
};

/**
 * createCompareList adds all possible line graph options to a select menu.
 * @param {string} graphListId The ID of the select element to populate.
 */
pagespeed.MpsConsole.prototype.createCompareList = function(graphListId) {
  var mpsconsole = this;
  var graphList = document.getElementById(graphListId);
  this.addMenuOption(graphListId, 'None', -1);
  for (var i = 0; i < this.LineGraphs_.length; i++) {
    this.addMenuOption(graphListId, this.LineGraphs_[i].title, i);
  }
};

/**
 * addMenuOption adds the given graph to the list of graphs
 * in the dropdown menu.
 * @param {string} title The name of the newly added graph.
 * @param {number} index The index corresponding to the graph.
 * @param {string} graphListId the ID of the select element to add options to.
 */
pagespeed.MpsConsole.prototype.addMenuOption =
    function(graphListId, title, index) {
  var option = document.createElement('option');
  option.value = index;
  option.text = title;
  document.getElementById(graphListId).add(option, null);
};

/**
 * setupAddWidget adds the functionality to hide and display
 * the options in the add Widget modal dialog.
 */
pagespeed.MpsConsole.prototype.setupAddWidget = function() {
  var hideFilter = document.getElementById('hide-filter');
  var addFilter = document.getElementById('add-filter');
  addFilter.onclick = function() {
    document.getElementById('date-filter').style.display = 'block';
    addFilter.style.display = 'none';
    hideFilter.style.display = 'inline';
  };
  hideFilter.onclick = this.hideAddFilters;
  var graphList = document.getElementById('add-graph-list');
  var mpsconsole = this;
  graphList.onchange = function() {
    var currentSelection = graphList.options[graphList.selectedIndex].innerHTML;
    if (currentSelection === 'Add a Metric') {
      document.getElementById('widget-title').value = '';
    } else {
      document.getElementById('widget-title').value = currentSelection;
      var compare = document.getElementById('mod-compare-with');
      if (mpsconsole.isHistogram(currentSelection)) {
        document.getElementById('start-date-filter').style.display = 'none';
        document.getElementById('end-date-label').innerHTML = 'Date';
        compare.style.display = 'none';
        compare.children[1].selectedIndex = 0;
      } else {
        document.getElementById('start-date-filter').style.display = 'block';
        document.getElementById('end-date-label').innerHTML = 'End Date: ';
        compare.style.display = 'block';
      }
    }
  };
  this.CreatePopupDatePicker('start-date-calendar-button', 'start-date');
  this.CreatePopupDatePicker('end-date-calendar-button', 'end-date');
};

/**
 * Create a PopupDatePicker for the given button and span.
 * @param {string} button The ID of the button that will trigger the picker.
 * @param {string} span The ID of the span that will hold the selected date.
 */
pagespeed.MpsConsole.prototype.CreatePopupDatePicker = function(button, span) {
  var picker = new goog.ui.PopupDatePicker();
  picker.render();
  picker.attach(document.getElementById(button));
  goog.events.listen(picker, 'change', function(e) {
    var date = picker.getDate();
    var dateString = date ? ('' + date.getYear() + '-' + (date.getMonth() + 1) +
        '-' + date.getDate()) : '';
    document.getElementById(span).innerHTML = dateString;
  });
}

/**
 * hideAddFilters hides the filter options in the Add Widget modal dialog.
 * It also sets the optional filters (date, etc.) to their defaults.

 */
pagespeed.MpsConsole.prototype.hideAddFilters = function() {
  document.getElementById('add-filter').style.display = 'block';
  document.getElementById('date-filter').style.display = 'none';
  document.getElementById('hide-filter').style.display = 'none';
  document.getElementById('start-date').innerHTML = '';
  document.getElementById('end-date').innerHTML = '';
  document.getElementById('granularity').value = '';
  document.getElementById('widget-title').value = '';
};

/**
 * hideEditFilters hides the filter options in the Edit Widget modal dialog.
 * It also sets the filter values to the default.
 */
pagespeed.MpsConsole.prototype.hideEditFilters = function() {
  document.getElementById('add-filter-edit').style.display = 'block';
  document.getElementById('date-filter-edit').style.display = 'none';
  document.getElementById('hide-filter-edit').style.display = 'none';
  document.getElementById('start-date-edit').innerHTML = '';
  document.getElementById('end-date-edit').innerHTML = '';
  document.getElementById('granularity-edit').value = '';
  document.getElementById('widget-title-edit').value = '';
};

/**
 * Reset the add widget to its default state.
 */
pagespeed.MpsConsole.prototype.resetAddWidget = function() {
  this.hideAddFilters();
  var graphs = ['add-graph-list', 'add-graph-compare-list'];
  for (var g in graphs) {
    g = graphs[g];
    if (document.getElementById(g)) {
      document.getElementById(g).selectedIndex = 0;
    }
  }
  document.getElementById('mod-compare-with').style.display = 'block';
};

/**
 * Reset the edit widget to its default state.
 */
pagespeed.MpsConsole.prototype.resetEditWidget = function() {
  this.hideEditFilters();
  var graphs = ['edit-graph-list', 'edit-graph-compare-list'];
  for (var g in graphs) {
    g = graphs[g];
    if (document.getElementById(g)) {
      document.getElementById(g).selectedIndex = 0;
    }
  }
};

/**
 * addGraph adds a new graph based on the user's selections.
 */
pagespeed.MpsConsole.prototype.addGraph = function() {
  var mpsconsole = this;
  var graphList = document.getElementById('add-graph-list');
  var selected = graphList.options[graphList.selectedIndex];
  var isHistogram = this.isHistogram(selected.innerHTML);
  var startTime = document.getElementById('start-date').innerHTML;
  var endTime = document.getElementById('end-date').innerHTML;
  // If startTime and endTime have not been specified by the user, use
  // the default values.
  if (startTime) {
    startTime = new Date(startTime).getTime();
  } else {
    startTime = 0;
  }
  // If no endtime is specified, make the current time the end time.
  if (!endTime) {
    endTime = new Date().getTime();
  } else {
    // Assume that the user wanted to include the specified end date.
    var num_ms_in_day = 86400000;
    endTime = new Date(endTime).getTime() + num_ms_in_day;
  }
  // The granularity is a number-type input, so no need to parseInt.
  var granularityMs = document.getElementById('granularity').value * 1000;
  // Default value: 3 seconds (same as default logging interval).
  if (granularityMs == '' || granularityMs == 0) {
    granularityMs = 3000;
  }
  var graphTitle = document.getElementById('widget-title').value;
  var graphList3 = document.getElementById('add-graph-compare-list');
  var graphsToQuery = [parseInt(selected.value, 10)];
  var compareGraph =
      parseInt(graphList3.options[graphList3.selectedIndex].value, 10);
  if (compareGraph >= 0) {
    graphsToQuery.push(compareGraph);
  }
  mpsconsole.getGraph(graphTitle, graphsToQuery, isHistogram, startTime,
                      endTime, granularityMs);
  mpsconsole.hideAddFilters();
};

/**
 * Toggle whether graphs auto-update.
 */
pagespeed.MpsConsole.prototype.toggleAutoUpdate = function() {
  if (this.updateInterval_) {
    this.stopAutoUpdate();
  } else {
    this.startAutoUpdate();
  }
};

/**
 * Turn off auto-update.
 */
pagespeed.MpsConsole.prototype.stopAutoUpdate = function() {
  clearInterval(this.updateInterval_);
  this.updateInterval_ = null;
  document.getElementById('auto-update').innerHTML = 'Auto-update off';
};

/**
 * Turn on auto-update.
 */
pagespeed.MpsConsole.prototype.startAutoUpdate = function() {
  var mpsconsole = this;
  mpsconsole.updateAllGraphs();
  this.updateInterval_ = setInterval(function() {
    mpsconsole.updateAllGraphs();
  }, this.UPDATE_INTERVAL_MS);
  document.getElementById('auto-update').innerHTML = 'Auto-update on';
};

/**
 * Update all the visible graphs.
 */
pagespeed.MpsConsole.prototype.updateAllGraphs = function() {
  var time = new Date().getTime();
  if (this.updatePaused_) {
    return;
  }
  for (var i = 0; i < this.graphs_.length; ++i) {
    if (!this.graphs_[i]) {
      // This graph has been deleted.
      continue;
    }
    if (this.histograms_[i]) {
      continue;
    }
    var title = document.getElementById('mod-title' + i).innerHTML;
    this.redrawLineGraph(i, title, this.graphMetrics_[i],
                         time - this.TIME_WINDOW_MS, time,
                         this.UPDATE_INTERVAL_MS, true);
  }
};

/**
 * getGraph issues an XHR to request data from the server for
 * the graphs with the given parameters.
 * @param {string} graphTitle The title of the graph to display.
 * @param {Array.<number>} graphIndex The indexes of the graphs queried.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {number} startTime The starting time of the data requested.
 * @param {number} endTime The ending time of the data requested.
 * @param {number} granularityMs The frequency of the datapoints requested.
 */
pagespeed.MpsConsole.prototype.getGraph =
    function(graphTitle, graphIndex, isHistogram, startTime, endTime,
    granularityMs) {
  var xhr = new XMLHttpRequest();
  var mpsConsole = this;
  var queryString = this.createQueryURI(isHistogram, graphIndex, startTime,
                                        endTime, granularityMs);
  xhr.onreadystatechange = function() {
    if (this.readyState != 4) {
      return;
    }
    if (this.status != 200 || this.responseText.length < 1 ||
        this.responseText[0] != '{') {
      document.getElementById('mod-error').style.display = 'block';
      return;
    }
    document.getElementById('mod-error').style.display = 'none';
    mpsConsole.scrapeData(JSON.parse(this.responseText), graphIndex,
                          isHistogram, endTime, graphTitle);
  };

  xhr.open('GET', queryString);
  xhr.send();
};

/**
 * createQueryURI determines whether the graph is a histogram or a line graph
 * and then creates a query URI string to send to mod_pagespeed_statistics_json
 * using the given parameters.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {Array.<number>|null} graphIndexes The indexes of the graphs queried.
 * @param {number} startTime The starting time of the data requested.
 * @param {number} endTime The ending time of the data requested.
 * @param {number} granularityMs The frequency of the datapoints requested.
 * @return {string} The URI to append to the query.
 */
pagespeed.MpsConsole.prototype.createQueryURI =
    function(isHistogram, graphIndexes, startTime, endTime, granularityMs) {
  var queryString = '/mod_pagespeed_statistics?json';
  queryString += '&start_time=' + startTime;
  queryString += '&end_time=' + endTime;
  queryString += '&granularity=' + granularityMs;

  if (!isHistogram) {
    queryString += '&var_titles=';
    for (var j = 0; j < graphIndexes.length; j++) {
      if (graphIndexes[j] < 0) {
        continue;
      }
      var var_data_needed = this.LineGraphs_[graphIndexes[j]].variables;
      for (var i = 0; i < var_data_needed.length; i++) {
        queryString += var_data_needed[i] + ',';
      }
    }
  } else {
    queryString += '&hist_titles=';
    for (var j = 0; j < graphIndexes.length; j++) {
      if (graphIndexes[j] < 0) {
        continue;
      }
      queryString += this.Histograms_[graphIndexes[j]].variable + ',';
    }
  }
  return queryString;
};


/**
 * loadMessagesData sends an AJAX request to scrape the
 * error messages at /mod_pagespeed_message. It then displays the error messages
 * in the appropriate div, creating it if necessary.
 */
pagespeed.MpsConsole.prototype.loadMessagesData = function() {
 var messages = document.getElementById('messages-div');
  var toggleMessages = document.getElementById('toggle-messages');
  if (messages.style.display == 'none') {
    // We don't have access to the console object, so set this here.
    var parseUrls = function(str) {
      // Note: Chrome appears to be *very* picky about regexes, and tended to
      // freeze rather than reporting a syntax error. It may be easier to debug
      // this in Firefox+Firebug.
      var url = new RegExp('\\b(http:\\/\\/[-A-Za-z0-9+&@#\\/%?=~_()|!:,.;]*' +
                           '[-A-Za-z0-9+&@#\\/%=~_()|])', 'g');
      str = str.replace(url, '<a href="$1">$1</a>');
      return str;
    }
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (this.readyState != 4) {
        return;
      }
      if (this.status != 200) {
        document.getElementById('mod-error').style.display = 'block';
        return;
      }
      document.getElementById('mod-error').style.display = 'none';
      var logText = this.responseText;
      var logTable = document.createElement('table');
      logTable.setAttribute('id', 'mod-table');
      logTable.setAttribute('cellspacing', '0');
      var currTr, currTd, currSection;
      currSection = document.createElement('thead');
      currTr = document.createElement('tr');
      var code = new RegExp('\\[([A-Z][a-z]+)\\]');
      var date = new RegExp('\\[([a-zA-Z]{3} [a-zA-Z]{3} [0-9]{1,2} ' +
                           '[0-9]{1,2}:[0-9]{2}:[0-9]{2} [0-9]{4})\\]');
      var pid = new RegExp('\\[([0-9]+)\\]');
      var message = new RegExp('\\[[0-9]+\\] (.*)$');
      var regexes = [code, date, message, pid];
      var headers = ['Code', 'Time', 'Message', 'PID'];
      var classes = ['code', 'time', 'message', 'pid'];
      for (var i = 0; i < headers.length; ++i) {
        currTd = document.createElement('th');
        currTd.innerHTML = headers[i];
        currTd.setAttribute('class', classes[i]);
        currTd.setAttribute('scope', 'col');
        currTr.appendChild(currTd);
      }
      currSection.appendChild(currTr);
      logTable.appendChild(currSection);
      currSection = document.createElement('tbody');
      var logArray = logText.replace(/\n?<\/?pre>/g, '').split('\n');
      for (var i = 0; i < logArray.length; ++i) {
        currTr = document.createElement('tr');
        for (var j = 0; j < regexes.length; ++j) {
          currTd = document.createElement('td');
          var matched = logArray[i].match(regexes[j]);
          if (matched) {
            if (regexes[j] === message) {
              currTd.innerHTML = parseUrls(matched[1]);
            } else {
              currTd.innerHTML = matched[1];
            }
            if (matched[1] === 'Info') {
              currTd.setAttribute('class', 'code info');
            } else if (matched[1] === 'Warning') {
              currTd.setAttribute('class', 'code warning');
            } else if (matched[1] === 'Error') {
              currTd.setAttribute('class', 'code error');
            } else if (matched[1] === 'Fatal') {
              currTd.setAttribute('class', 'code fatal');
            }
          }
          if (regexes[j] !== code) {
            currTd.setAttribute('class', classes[j]);
          }
          currTr.appendChild(currTd);
        }
        currSection.appendChild(currTr);
      }
      logTable.appendChild(currSection);
      document.getElementById('messages').innerHTML = '';
      document.getElementById('messages').appendChild(logTable);
      messages.style.display = 'block';
      toggleMessages.innerHTML = 'Hide recent messages';
    };
    xhr.open('GET', '/mod_pagespeed_message');
    xhr.send();
  } else {
    messages.style.display = 'none';
    toggleMessages.innerHTML = 'Show recent messages';
  }
};

/**
 * Given a graph title, this function returns whether it is a histogram.
 * @param {string} graphTitle The name of the graph.
 * @return {boolean} Whether the graph is a histogram.
 */
pagespeed.MpsConsole.prototype.isHistogram = function(graphTitle) {
  for (var i = 0; i < this.Histograms_.length; i++) {
    if (this.Histograms_[i].title == graphTitle) return true;
  }
  return false;
};

/**
 * scrapeData scrapes data out of the text passed to it, which is presumed to be
 * from an AJAX call to /mod_pagespeed_statistics?json&... It then processes
 * the data and calls helper functions to redraw the graphs.
 * @param {pagespeed.MpsConsole.JSONData} data The data, parsed from the JSON
 *     response.
 * @param {Array.<number>} graphIndexes The graph queried.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {number} endTime The ending time of the data requested, displayed if
 *   the user has queried a histogram.
 * @param {string} graphTitle The title to display for the graph.
 */
pagespeed.MpsConsole.prototype.scrapeData =
    function(data, graphIndexes, isHistogram, endTime, graphTitle) {
  if (isHistogram) {
    this.updateHistogram(data['histograms'], graphIndexes[0], endTime);
  } else {
    var timeSeriesData = [];
    for (var i = 0; i < graphIndexes.length; ++i) {
      var graphIndex = graphIndexes[i];
      if (graphIndex < 0) {
        continue;
      }
      if (this.LineGraphs_[graphIndex].type !=
          this.LineGraphTypes_.MULTIPLE_RAW_VALUES &&
          this.LineGraphs_[graphIndex].type !=
          this.LineGraphTypes_.MULTIPLE_RATE) {
        timeSeriesData.push(this.computeTimeSeries(data['variables'],
                                                   data['timestamps'],
                                                   graphIndex));
      } else {
        timeSeriesData = timeSeriesData.concat(this.computeMultipleTimeSeries(
                  data['variables'], data['timestamps'], graphIndex));
      }
    }
    this.updateLineGraph(timeSeriesData, data['timestamps'], graphIndexes,
                         graphTitle, -1);
  }
};

/**
 * computeTimeSeries goes through the mod_pagespeed_statistics variables scraped
 * by scrapeData and calculates useful values.
 * Some values are calculated by diffing two consecutive data entries.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A list of the
 *     timestamps scraped.
 * @param {number} graphIndex The line graph queried.
 * @return {Array.<number>} The calculated values.
 */
pagespeed.MpsConsole.prototype.computeTimeSeries =
    function(data, timestamps, graphIndex) {
  var timeSeriesData = [];
  var variablesNeeded = this.LineGraphs_[graphIndex].variables;
  var graphTitle = this.LineGraphs_[graphIndex].title;
  var type = this.LineGraphs_[graphIndex].type;
  for (var i = 1; i < timestamps.length; i++) {
    var previous_timestamp_ms = i - 1;
    var current_timestamp_ms = i;
    if (type == this.LineGraphTypes_.RATIO_OVER_SUM) {
      var total = data[variablesNeeded[0]][current_timestamp_ms] +
                  data[variablesNeeded[1]][current_timestamp_ms] -
                  data[variablesNeeded[0]][previous_timestamp_ms] -
                  data[variablesNeeded[1]][previous_timestamp_ms];
      timeSeriesData[i - 1] =
            this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                           variablesNeeded[0], total);
    } else if (type == this.LineGraphTypes_.RATIO) {
      timeSeriesData[i - 1] = this.ratioStatWithTimeSeriesVar(
            data, current_timestamp_ms, previous_timestamp_ms,
            variablesNeeded[0], variablesNeeded[1]);
    } else if (type == this.LineGraphTypes_.RATE) {
      timeSeriesData[i - 1] =
            this.ratioStatElapsedTime(data, current_timestamp_ms,
                                      previous_timestamp_ms, variablesNeeded[0],
                                      timestamps);
    } else if (type == this.LineGraphTypes_.RAW_VALUE) {
      timeSeriesData[i - 1] =
           data[variablesNeeded[0]][current_timestamp_ms];
    }
  }
  return timeSeriesData;
};

/**
 * computeMultipleTimeSeries goes through the scraped variables and computes
 * one time series for each variable needed by a given multi-variable graph.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A list of the
 *     timestamps scraped.
 * @param {number} graphIndex The line graph queried.
 * @return {Array.<Array.<number>>} The calculated values.
 */
pagespeed.MpsConsole.prototype.computeMultipleTimeSeries =
    function(data, timestamps, graphIndex) {
  var variablesNeeded = this.LineGraphs_[graphIndex].variables;
  var timeSeriesData = new Array(variablesNeeded.length);
  if (this.LineGraphs_[graphIndex].type ==
          this.LineGraphTypes_.MULTIPLE_RAW_VALUES) {
    for (var i = 0; i < variablesNeeded.length; i++) {
      timeSeriesData[i] = [];
    }
    for (var i = 1; i < timestamps.length; i++) {
      for (var j = 0; j < variablesNeeded.length; j++) {
        timeSeriesData[j][i - 1] =
          data[variablesNeeded[j]][i];
      }
    }
  } else if (this.LineGraphs_[graphIndex].type ==
      this.LineGraphTypes_.MULTIPLE_RATE) {
    for (var i = 0; i < variablesNeeded.length; i++) {
      timeSeriesData[i] = [];
    }
    for (var i = 1; i < timestamps.length; i++) {
      for (var j = 0; j < variablesNeeded.length; j++) {
        var previous_timestamp = i - 1;
        var current_timestamp = i;
        timeSeriesData[j][i - 1] = this.ratioStatElapsedTime(data,
            current_timestamp, previous_timestamp, variablesNeeded[j],
            timestamps);
      }
    }
  }
  timeSeriesData.push([]);
  var totalIndex = timeSeriesData.length - 1;
  for (var i = 0; i < timeSeriesData[0].length; ++i) {
    var total = 0;
    for (var j = 0; j < totalIndex; ++j) {
      total += timeSeriesData[j][i];
    }
    timeSeriesData[totalIndex].push(total);
  }
  return timeSeriesData;
};

/**
 * ratioStat calculates the ratio of two values over some common denominator.
 * It prevents NaN errors by returning 0 if the denominator is 0.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {number} current_timestamp_ms The time of the current scrape.
 * @param {number} previous_timestamp_ms The time of the previous scrape.
 * @param {string} property The parameter to compare across the two datasets.
 * @param {number} denominator The value by which to divide the difference.
 * @return {number} The computed value.
 */
pagespeed.MpsConsole.prototype.ratioStat =
    function(data, current_timestamp_ms, previous_timestamp_ms,
             property, denominator) {
  if (denominator == 0) {
    return 0;
  } else {
    return (data[property][current_timestamp_ms] -
        data[property][previous_timestamp_ms]) / denominator;
  }
};

/**
 * ratioStatElapsedTime calls ratioStat with the denominator being
 * the elapsed time in seconds.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {number} current_timestamp_ms The time of the current scrape.
 * @param {number} previous_timestamp_ms The time of the previous scrape.
 * @param {string} property The parameter to compare across the two datasets.
 * @param {pagespeed.MpsConsole.timestampData} timestamps The list of timestamps
 *     queried.
 * @return {number} The computed value.
 */
pagespeed.MpsConsole.prototype.ratioStatElapsedTime =
    function(data, current_timestamp_ms, previous_timestamp_ms, property,
             timestamps) {
  var timeDifference = (timestamps[current_timestamp_ms] -
      timestamps[previous_timestamp_ms]) / 1000;
  return this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                        property, timeDifference);
};

/**
 * ratioStat with the denominator being a different variable's time series data.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {number} current_timestamp_ms The time of the current scrape.
 * @param {number} previous_timestamp_ms The time of the previous scrape.
 * @param {string} numeratorVar The variable to compare across the two datasets.
 * @param {string} denominatorVar The variable to divide by.
 * @return {number} The computed value.
 */
pagespeed.MpsConsole.prototype.ratioStatWithTimeSeriesVar =
  function(data, current_timestamp_ms, previous_timestamp_ms, numeratorVar,
           denominatorVar) {
  var denominator = data[denominatorVar][current_timestamp_ms] -
      data[denominatorVar][previous_timestamp_ms];
  return this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                        numeratorVar, denominator);
};

/**
 * updateHistogram adds new data to the appropriate
 * DataTable. It then draws/redraws the histogram to
 * include the new data.
 * @param {pagespeed.MpsConsole.histogramData}
 *     histData The parsed histogram data.
 * @param {number} graphIndex The index of the graph queried.
 * @param {number} endTime The ending time of the data requested.
 */
pagespeed.MpsConsole.prototype.updateHistogram =
    function(histData, graphIndex, endTime) {
  var variableName = this.Histograms_[graphIndex].variable;
  var graphTitle = this.Histograms_[graphIndex].title;
  var time = new Date(endTime);
  time = time.toDateString() + ', ' + time.toLocaleTimeString();
  var dt = this.createHistogramDataTable(variableName);
  var arrayOfBarsInfo = histData[variableName];
  if (!arrayOfBarsInfo) {
    this.displayNoDataFound(graphTitle);
    return;
  }
  for (var h = 0; h < arrayOfBarsInfo.length; h++) {
    var lowerBound = Math.round(arrayOfBarsInfo[h.toString()].lowerBound);
    var upperBound = Math.round(arrayOfBarsInfo[h.toString()].upperBound);
    if (!upperBound) {
      upperBound = '∞';
    }
    if (!lowerBound) {
      lowerBound = 0;
    }
    dt.addRow([
      '[' + lowerBound + ', ' + upperBound + ')',
      arrayOfBarsInfo[h.toString()].count
    ]);
  }
  if (dt.getNumberOfRows()) {
    var options = this.barGraphOptions_;
    options['height'] = ((arrayOfBarsInfo.length + 1) * 20 > 255) ? 557 : 255;
    options['chartArea']['height'] = options['height'] - 30;
    var graph = this.getBarGraph(graphTitle, time);
    graph.draw(dt, options);
    this.dataTables_.push(dt);
    this.graphs_.push(graph);
    this.graphMetrics_.push([graphIndex]);
    this.histograms_.push(true);
  }
};

/**
 * updateLineGraph takes new data and adds it to the
 * appropriate DataTable. It then draws/redraws the line graph to
 * include the new data.
 * @param {Array.<Array.<number>>} varData The data to update the
 *     line graphs with.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A list of timestamps
 * representing when the data was logged.
 * @param {Array.<number>} graphIndexes The index of the line graph queried.
 * @param {string} graphTitle The display title of the graph.
 * @param {number} number The number of the graph on the page.
 */
pagespeed.MpsConsole.prototype.updateLineGraph =
    function(varData, timestamps, graphIndexes, graphTitle, number) {
  var colTitles = [];
  for (var i = 0; i < graphIndexes.length; ++i) {
    if (this.LineGraphs_[graphIndexes[i]].type !=
        this.LineGraphTypes_.MULTIPLE_RAW_VALUES &&
        this.LineGraphs_[graphIndexes[i]].type !=
        this.LineGraphTypes_.MULTIPLE_RATE) {
      colTitles.push(this.LineGraphs_[graphIndexes[i]].title);
    } else {
      colTitles = colTitles.concat(this.LineGraphs_[graphIndexes[i]].titles);
    }
  }

  var dt = this.createLineGraphDataTable(colTitles[0]);
  for (var i = 0; i < timestamps.length; i++) {
    dt.addRow([new Date(parseInt(timestamps[i], 10)), varData[0][i]]);
  }

  for (var i = 1; i < varData.length; i++) {
    if (graphIndexes[i] < 0) {
      continue;
    }
    this.addDataSetToLineGraph(varData[i], colTitles[i], dt);
  }

  var graph;
  if (dt.getNumberOfRows()) {
    if (number < 0) {
      graph = this.getLineGraph(graphTitle);
      this.graphs_.push(graph);
      this.dataTables_.push(dt);
      this.graphMetrics_.push(graphIndexes);
      this.histograms_.push(false);
    } else {
      this.dataTables_[number] = dt;
      this.graphMetrics_[number] = graphIndexes;
      graph = this.graphs_[number];
    }
    graph.draw(dt, this.lineGraphOptions_);
  } else {
    this.displayNoDataFound(graphTitle);
  }
  if (this.updatePaused_) {
    this.updatePaused_ = false;
    this.startAutoUpdate();
  }
};

/**
 * updateMultipleDataSetLineGraph draws a line graph that has several different
 * data sets graphed on the same axes. This is different from the compare
 * graphs functionality because these graphs by definition have more than one
 * data set associated with them.
 * @param {Array.<Array.<number>>} varData The scraped variables.
 * @param {Array.<number>} timestamps A list of the
 *     timestamps scraped.
 * @param {number} graphIndex The index of the base line graph.
 */
pagespeed.MpsConsole.prototype.updateMultipleDataSetLineGraph =
    function(varData, timestamps, graphIndex) {
  var graphTitle = this.LineGraphs_[graphIndex].title;
  var dt = this.createLineGraphDataTable(graphTitle);
  for (var i = 0; i < timestamps.length; i++) {
    dt.addRow([new Date(parseInt(timestamps[i], 10)), varData[0][i]]);
  }
  for (var i = 1; i < varData.length; i++) {
    this.addDataSetToLineGraph(varData[i],
                               this.LineGraphs_[graphIndex].variables[i], dt);
  }
};

/**
 * addDataSetToLineGraph takes a set of variable data and adds it to the
 * appropriate line graph so that two or more datasets can be graphed on
 * the same axes.
 * @param {Array.<number>} varData The data set to be graphed.
 * @param {string} legendTitle The text describing the line graph being drawn.
 * @param {Object} dt The datatable of the line graph.
 */
pagespeed.MpsConsole.prototype.addDataSetToLineGraph =
    function(varData, legendTitle, dt) {
  var newColumn = dt.getNumberOfColumns();
  dt.addColumn('number', legendTitle);
  for (var i = 0; i < varData.length; i++) {
    dt.setCell(i, newColumn, varData[i]);
  }
};

/**
 * displayNoDataFound displays a message on the console stating that no data
 * matched the user's query.
 * @param {string} graphTitle The title of the graph queried.
 */
pagespeed.MpsConsole.prototype.displayNoDataFound = function(graphTitle) {
  var noMatch = document.createElement('div');
  noMatch.setAttribute('class', 'mod-no-match');
  noMatch.innerHTML = 'No data matches your query for ' + graphTitle;
  document.getElementById('container').appendChild(noMatch);
  setTimeout(function() {noMatch.parentNode.removeChild(noMatch);}, 10000);
};

/**
 * createGraphDiv creates the necessary DOM elements for a graph, and returns
 * the div in which the graph will be drawn.
 * @param {string} title The title of the graph.
 * @param {(string|null)} time The timestamp, if the graph is a histogram.
 * @return {Element} the div in which to draw the graph.
 */
pagespeed.MpsConsole.prototype.createGraphDiv = function(title, time) {
  var wholeDiv = document.createElement('div');
  wholeDiv.setAttribute('class', 'mod-widgets');
  this.createGraphTitleBar(wholeDiv, title, time);
  var graph = document.createElement('div');
  graph.number = this.graphs_.length;
  graph.setAttribute('class', 'graph');
  wholeDiv.appendChild(graph);
  var container = document.getElementById('container');
  container.appendChild(wholeDiv);
  return graph;
};

/**
 * Creates the title and dropdown menu of each graph.
 * @param {Element} wholeDiv The div in which the graph and title is drawn.
 * @param {string} title The title of the type graph.
 * @param {(string|null)} time The timestamp, if the graph is a histogram.
 */
pagespeed.MpsConsole.prototype.createGraphTitleBar =
    function(wholeDiv, title, time) {
  var topBar = document.createElement('div');
  topBar.setAttribute('class', 'mod-widgets-topbar');
  // There are two concepts of title here: the display title, chosen by the
  // user, and the graph title, which is the name of the type of graph (as
  // in LineGraphs_ or Histograms_). If the user does not specify a display
  // title, the default graph title will be used.
  var graphTitle = document.createElement('span');
  graphTitle.setAttribute('class', 'title');
  graphTitle.setAttribute('id', 'mod-title' + this.graphs_.length);
  var displayTitle = '';
  var displayTitleInput = document.getElementById('widget-title');
  if (displayTitleInput) {
    displayTitle = goog.string.htmlEscape(displayTitleInput.value);
  }
  if (displayTitle == '') {
    displayTitle += title;
  }
  if (time) {
    displayTitle += ' on ' + time;
  }
  graphTitle.innerHTML = displayTitle;
  topBar.appendChild(graphTitle);
  var selectorButton = document.createElement('button');
  selectorButton.setAttribute('class', 'graphselector menuButton');
  selectorButton.setAttribute('id', 'mod-menu-button' + this.graphs_.length);
  var menuDiv = document.createElement('div');
  menuDiv.setAttribute('class', 'goog-menu');
  menuDiv.setAttribute('for', selectorButton.id);
  menuDiv.style.display = 'none';
  if (!this.isHistogram(title)) {
    menuDiv.appendChild(this.createEditGraphButton(title, this.graphs_.length));
  }
  menuDiv.appendChild(
      this.createRemoveGraphButton(wholeDiv, title, this.graphs_.length));
  topBar.appendChild(menuDiv);
  var pm = new goog.ui.PopupMenu();
  pm.setToggleMode(true);
  pm.decorate(menuDiv);
  pm.attach(selectorButton,
            goog.positioning.Corner.BOTTOM_LEFT,
            goog.positioning.Corner.TOP_LEFT);
  topBar.appendChild(selectorButton);
  wholeDiv.appendChild(topBar);
};

/**
 * createRemoveGraphButton creates the link that deletes a graph.
 * @param {Element} div The div in which the graph is drawn.
 * @param {string} title The name of the type of graph.
 * @param {number} graphNumber The number of the graph.
 * @return {Element} The element containing the remove button option.
 */
pagespeed.MpsConsole.prototype.createRemoveGraphButton =
    function(div, title, graphNumber) {
  var mpsconsole = this;
  var removeButton = document.createElement('div');
  removeButton.setAttribute('class', 'removeButton goog-menuitem');
  removeButton.innerHTML = 'Remove ';
  removeButton.onclick = function() {
    div.parentNode.removeChild(div);
    removeButton.parentNode.removeChild(removeButton);
    delete mpsconsole.dataTables_[graphNumber];
    delete mpsconsole.graphs_[graphNumber];
    delete mpsconsole.graphMetrics_[graphNumber];
    delete mpsconsole.histograms_[graphNumber];
  };
  return removeButton;
};

/**
 * createEditGraphButton creates the link that edits a graph.
 * @param {string} title The name of the type of graph.
 * @param {number} number The number of the graph.
 * @return {Element} The element containing the edit button option.
 */
pagespeed.MpsConsole.prototype.createEditGraphButton =
    function(title, number) {
  var mpsconsole = this;
  var editButton = document.createElement('div');
  editButton.setAttribute('class', 'editButton goog-menuitem unimplemented');
  editButton.innerHTML = 'Edit';
  editButton.onclick = function() {
    if (mpsconsole.updateInterval_) {
      mpsconsole.stopAutoUpdate();
      mpsconsole.updatePaused_ = true;
      // We'll restart updating after the graph is added.
    }
    mpsconsole.currentlyEditing_ = number;
    mpsconsole.editDialog_.setVisible(true);
    document.getElementById('edit-graph-list').selectedIndex =
        mpsconsole.graphMetrics_[number][0] + 1;
    if (mpsconsole.graphMetrics_[number][1]) {
      document.getElementById('edit-graph-compare-list').selectedIndex =
          mpsconsole.graphMetrics_[number][1] + 1;
    }
    var currentTitle = document.getElementById('mod-title' + number).innerHTML;
    document.getElementById('widget-title-edit').value = currentTitle;
    this.CreatePopupDatePicker('start-date-calendar-button-edit',
        'start-date-edit');
    this.CreatePopupDatePicker('end-date-calendar-button-edit',
        'end-date-edit');
    var hideFilter = document.getElementById('hide-filter-edit');
    var addFilter = document.getElementById('add-filter-edit');
    addFilter.onclick = function() {
    document.getElementById('date-filter-edit').style.display = 'block';
      addFilter.style.display = 'none';
      hideFilter.style.display = 'inline';
    };
    hideFilter.onclick = this.hideAddFilters;
  };
  return editButton;
};

/**
 * getLineGraph creates a new line graph and the div in which it is displayed.
 * @param {string} title The title of the graph.
 * @return {Object} The graph.
 */
pagespeed.MpsConsole.prototype.getLineGraph = function(title) {
  var g = new google.visualization.LineChart(this.createGraphDiv(title, null));
  return g;
};

/**
 * getBarGraph creates a new bar graph and the div in which it is displayed.
 * @param {string} title The title of the graph.
 * @param {string} time The time if the graph is a histogram, null otherwise.
 * @return {Object} The graph.
 */
pagespeed.MpsConsole.prototype.getBarGraph = function(title, time) {
  return new google.visualization.BarChart(this.createGraphDiv(title, time));
};

/**
 * createLineGraphDataTable creates a new google.visualization.DataTable
 * and returns it. Each DataTable has two columns: a timestamp
 * (represented as a number, time elapsed in s) and the value at that time,
 * also a number. This DataTable is meant for a line graph DataView.
 * @param {string} title The name of the variable being measured.
 * @return {Object} The data table.
 */
pagespeed.MpsConsole.prototype.createLineGraphDataTable = function(title) {
  var dt = new google.visualization.DataTable();
  dt.addColumn('datetime', 'Time');
  dt.addColumn('number', title);
  return dt;
};

/**
 * createHistogramDataTable creates a new
 * google.visualization.DataTable and returns it.
 * Each DataTable has two columns: a string and a number.
 * This DataTable is meant for a bar graph DataView.
 * @param {string} title The name of the variable being measured.
 * @return {Object} The data table.
 */
pagespeed.MpsConsole.prototype.createHistogramDataTable = function(title) {
  var dt = new google.visualization.DataTable();
  dt.addColumn('string', 'Bounds');
  dt.addColumn('number', title);
  return dt;
};

/**
 * Detect whether the browser supports localStorage.
 * From http://diveintohtml5.info/storage.html
 * @return {boolean} Whether the browser supports localStorage.
 */
pagespeed.MpsConsole.prototype.hasLocalStorage = function() {
  try {
    return 'localStorage' in window && window['localStorage'] !== null;
  } catch (e) {
    return false;
  }
};

/**
 * Save the graph configuration to localStorage if possible.
 */
pagespeed.MpsConsole.prototype.saveGraphs = function() {
  if (!this.hasLocalStorage()) {
    return false;
  }
  var localStorage = window['localStorage'];
  var graphTitles = [];
  for (var i = 0; i < this.graphs_.length; ++i) {
    if (this.graphs_[i]) {
      graphTitles.push(document.getElementById('mod-title' + i).innerHTML);
    }
  }
  var dataTables = [];
  var graphMetrics = [];
  var histograms = [];
  for (var i = 0; i < this.dataTables_.length; ++i) {
    if (this.dataTables_[i]) {
      dataTables.push(this.dataTables_[i].toJSON());
      graphMetrics.push(this.graphMetrics_[i]);
      histograms.push(this.histograms_[i]);
    }
  }
  try {
    localStorage['mps-autoUpdate'] = JSON.stringify(
        this.updateInterval_ ? true : false);
    localStorage['mps-graphTitles'] = JSON.stringify(graphTitles);
    localStorage['mps-dataTables'] = JSON.stringify(dataTables);
    localStorage['mps-graphMetrics'] = JSON.stringify(graphMetrics);
    localStorage['mps-histograms'] = JSON.stringify(histograms);
  } catch (QuotaExceededError) {
    return false;
  }
  return true;
};

/**
 * Load the graph configuration from localStorage if possible.
 */
pagespeed.MpsConsole.prototype.loadGraphs = function() {
  if (!this.hasLocalStorage()) {
    return false;
  }
  var localStorage = window['localStorage'];
  if (!localStorage['mps-autoUpdate'] || !localStorage['mps-graphTitles'] ||
      !localStorage['mps-dataTables'] || !localStorage['mps-graphMetrics'] ||
      !localStorage['mps-histograms']) {
    return false;
  }
  this.graphMetrics_ = JSON.parse(localStorage['mps-graphMetrics']);
  this.histograms_ = JSON.parse(localStorage['mps-histograms']);
  var dataTables = JSON.parse(localStorage['mps-dataTables']);
  var graphTitles = JSON.parse(localStorage['mps-graphTitles']);
  for (var i = 0; i < graphTitles.length; ++i) {
    var graph, options, title, time;
    var splitTitle = graphTitles[i].split(' on ');
    if (this.histograms_[i] && splitTitle[1]) {
      title = splitTitle[0];
      time = splitTitle[1];
    } else {
      title = graphTitles[i];
      time = null;
    }
    var dt = new google.visualization.DataTable(dataTables[i]);
    if (time) {
      graph = this.getBarGraph(title, time);
      options = this.barGraphOptions_;
      options['height'] = ((dt.getNumberOfRows() + 1) * 20 > 255) ? 557 : 255;
      options['chartArea']['height'] = options['height'] - 30;
    } else {
      graph = this.getLineGraph(title);
      options = this.lineGraphOptions_;
    }
    graph.draw(dt, options);
    this.dataTables_.push(dt);
    this.graphs_.push(graph);
  }
  if (JSON.parse(localStorage['mps-autoUpdate'])) {
    this.startAutoUpdate();
  }
  return true;
};
