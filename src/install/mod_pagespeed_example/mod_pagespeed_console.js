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
 */

'use strict';

// Exporting functions using quoted attributes to prevent js compiler
//     from renaming them.
// See https://cs.corp.google.com/#google3/net/instaweb/rewriter/
//    delay_images.js
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];
window['goog'] = window['goog'] || {};
var goog = window['goog'];

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
    RAW_VALUE: 'raw_value'
  };

  /**
   * A list of the different line graph titles, the variables that
   * the line graph data consists of, and the type of graph, which determines
   * how the data points should be calculated from the given variables.
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
    {title: 'Flushes per Second', type: this.LineGraphTypes_.RATE,
     variables: ['num_flushes']},
    {title: 'Fallback Responses Served per Second',
     type: this.LineGraphTypes_.RATE,
     variables: ['num_fallback_responses_served']},
    {title: 'Rewrites Executed per Second', type: this.LineGraphTypes_.RATE,
     variables: ['num_rewrites_executed']},
    {title: 'Rewrites Dropped per Second', type: this.LineGraphTypes_.RATE,
     variables: ['num_rewrites_dropped']},
    {title: 'Resource 404s per Second', type: this.LineGraphTypes_.RATE,
     variables: ['resource_404_count']},
    {title: 'Slurp 404s per Second', type: this.LineGraphTypes_.RATE,
     variables: ['slurp_404_count']},
    {title: 'Ongoing Image Rewrites', type: this.LineGraphTypes_.RAW_VALUE,
     variables: ['image_ongoing_rewrites']}
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
   * An enum specifying the indexes of the line graphs.
   * @enum {number}
   */
  this.LineGraphIndexes = {
    PERCENT_CACHE_HITS: 0,
    SERF_BYTES_FETCHED: 1,
    AVG_LOAD_TIME: 2,
    QUERIES_PER_SEC: 3,
    NUM_FLUSHES: 4,
    FALLBACK_RESPONSES: 5,
    REWRITES_EXECUTED: 6,
    REWRITES_DROPPED: 7,
    RESOURCE_404S: 8,
    SLURP_404S: 9,
    ONGOING_IMG_REWRITES: 10
  };

  /**
   * An enum specifying the indexes of the histograms.
   * @enum {number}
   */
  this.HistogramIndexes = {
    HTML_TIME: 0,
    REWRITE_LATENCY: 1,
    PAGESPEED_RESOURCE_LATENCY: 2,
    BACKEND_FETCH_FIRST_BYTE_LATENCY: 3,
    MEMCACHED_GET_COUNT: 4,
    MEMCACHED_HIT_LATENCY: 5,
    MEMCACHED_INSERT_LATENCY: 6,
    MEMCACHED_INSERT_SIZE: 7,
    MEMCACHED_LOOKUP_SIZE: 8
  };
};

/**
 * Runs the console.
 * @return {pagespeed.MpsConsole} The console object.
 */
pagespeed.initConsole = function() {
  var mpsconsole = new pagespeed.MpsConsole();
  mpsconsole.createDivs();
  // Hardcoded in to show certain graphs on page load.
  var isHistogram = true;
  mpsconsole.getGraph(mpsconsole.LineGraphIndexes.NUM_FLUSHES, !isHistogram,
                      0, new Date().getTime(), 5000);
  mpsconsole.getGraph(mpsconsole.HistogramIndexes.HTML_TIME, isHistogram,
                      0, new Date().getTime(), 5000);
  return mpsconsole;
};

// Export this so the compiler doesn't rename it.
pagespeed['initConsole'] = pagespeed.initConsole;

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
};

/**
 * createAddWidgetDialog adds functionality to the add Widget modal dialog
 * by calling various helper functions. Uses the closure library for modal
 * dialog implementation.
 */
pagespeed.MpsConsole.prototype.createAddWidgetDialog = function() {
  var dialog1 = new goog.ui.Dialog();
  var addWidgetDialog = document.getElementById('add-widget');
  this.createGraphList();
  dialog1.setContent(addWidgetDialog.innerHTML);
  dialog1.setTitle('Add Widget');
  addWidgetDialog.parentNode.removeChild(addWidgetDialog);
  var mpsconsole = this;
  goog.events.listen(dialog1, goog.ui.Dialog.EventType.SELECT, function(e) {
  // e.key is the name of the button pressed - in this case, ok or cancel.
    if (e.key == 'ok') {
      mpsconsole.addGraph();
    }
  });
  // Causes modal popup to display when 'Add Widget' button is clicked.
  document.getElementById('modal-launcher').onclick = function() {
    dialog1.setVisible(true);
    mpsconsole.createFiltersInAddWidget();
  };
};

/**
 * createGraphList adds functionality to the dropdown list of possible graphs
 * to show and the associated default title.
 */
pagespeed.MpsConsole.prototype.createGraphList = function() {
  var mpsconsole = this;
  var graphList = document.getElementById('graph-list');
  var DEFAULT_OPTION = 'Add a Metric';
  this.addOption(DEFAULT_OPTION, -1);
  graphList.onchange = function() {
    var currentSelection = graphList.options[graphList.selectedIndex].value;
    if (currentSelection == DEFAULT_OPTION) {
      document.getElementById('widget-title').value = '';
    } else {
      document.getElementById('widget-title').value = currentSelection;
      // If a histogram is queried, the user chooses a single date, which is
      // stored in end date. If a line graph is queried, the user chooses a
      // start and end date.
      if (mpsconsole.isHistogram(currentSelection)) {
        document.getElementById('start-date-filter').style.display = 'none';
        document.getElementById('end-date-label').innerHTML = 'Date';
      } else {
        document.getElementById('start-date-filter').style.display = 'block';
        document.getElementById('end-date-label').innerHTML = 'End Date';
      }
    }
  };
  this.populateGraphList();
};

/**
 * populateGraphList adds all the names of all graphs to the dropdown menu.
 */
pagespeed.MpsConsole.prototype.populateGraphList = function() {
  for (var i = 0; i < this.LineGraphs_.length; i++) {
    this.addOption(this.LineGraphs_[i].title, i);
  }
  for (var i = 0; i < this.Histograms_.length; i++) {
    this.addOption(this.Histograms_[i].title, i);
  }
};

/**
 * addOption adds the given graph to the list of graphs
 * in the dropdown menu.
 * @param {string} title The name of the newly added graph.
 * @param {number} index The index corresponding to the graph.
 */
pagespeed.MpsConsole.prototype.addOption = function(title, index) {
  var option = document.createElement('option');
  option.value = index;
  option.text = title;
  document.getElementById('graph-list').add(option, null);
};

/**
 * createFiltersInAddWidget adds the functionality to hide and display
 * the filtering options in the add Widget modal dialog.
 */
pagespeed.MpsConsole.prototype.createFiltersInAddWidget = function() {
  var hideFilter = document.getElementById('hide-filter');
  var addFilter = document.getElementById('add-filter');
  addFilter.onclick = function() {
    document.getElementById('date-filter').style.display = 'block';
    addFilter.style.display = 'none';
    hideFilter.style.display = 'inline';
  };
  hideFilter.onclick = this.hideFilters;
};

/**
 * hideFilters hides the filter options in the Add Widget modal dialog.
 * It also sets the filter values to the default.
 */
pagespeed.MpsConsole.prototype.hideFilters = function() {
  document.getElementById('add-filter').style.display = 'block';
  document.getElementById('date-filter').style.display = 'none';
  document.getElementById('hide-filter').style.display = 'none';
  document.getElementById('start-date').value = '';
  document.getElementById('end-date').value = '';
  document.getElementById('granularity').value = '';
};

/**
 * createAddGraphButton gives the user the ability to display another
 * graph on the page. A dropdown menu is created so that the user can
 * select which other graphs to display.
 */
pagespeed.MpsConsole.prototype.addGraph = function() {
  var mpsconsole = this;
  var graphList = document.getElementById('graph-list');
  var selected = graphList.options[graphList.selectedIndex];
  var isHistogram = this.isHistogram(selected.innerHTML);
  // TODO(sarahdw, bvb): Better way of getting date ranges.
  var startTime = document.getElementById('start-date').value;
  var endTime = document.getElementById('end-date').value;
  // If startTime and endTime have not been specified by the user, use
  // the default values.
  if (startTime != 0) { // 0 is the default value for startTime.
    startTime = new Date(startTime).getTime();
  }
  // If no endtime is specified, make the current time the end time.
  if (endTime == 0) {
    endTime = new Date().getTime();
  } else {
    // Assume that the user wanted to include the specified end date.
    var num_ms_in_day = 86400000;
    endTime = new Date(endTime).getTime() + num_ms_in_day;
  }
  // TODO(bvb, sarahdw): Make sure that granularity is of type number.
  var granularityMs = document.getElementById('granularity').value * 1000;
  // Default value: 3 seconds (same as default logging interval).
  if (granularityMs == '' || granularityMs == 0) {
    granularityMs = 3000;
  }
  mpsconsole.getGraph(selected.value, isHistogram, startTime, endTime,
                      granularityMs);
  mpsconsole.hideFilters();
};

/**
 * getGraph issues an XHR to request data from the server for
 * the graphs with the given parameters.
 * @param {number} graphIndex The index of the graph queried.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {number} startTime The starting time of the data requested.
 * @param {number} endTime The ending time of the data requested.
 * @param {number} granularityMs The frequency of the datapoints requested.
 */
pagespeed.MpsConsole.prototype.getGraph =
    function(graphIndex, isHistogram, startTime, endTime, granularityMs) {
  var xhr = new XMLHttpRequest();
  var mpsConsole = this;
  var queryString = this.createQueryURI(isHistogram, graphIndex, startTime,
                                        endTime, granularityMs);
  // TODO(bvb,sarahdw): detect when server returns 'statistics not enabled'
  xhr.onreadystatechange = function() {
    if (this.readyState != 4) {
      return;
    }
    if (this.status != 200) {
      document.getElementById('error').style.display = 'block';
      return;
    }
    document.getElementById('error').style.display = 'none';
    mpsConsole.scrapeData(/** @type {?} */ (JSON.parse(this.responseText)),
                          graphIndex,
                          isHistogram, endTime);
  };

  xhr.open('GET', queryString);
  xhr.send();
};

/**
 * createQueryURI determines whether the graph is a histogram or a line graph
 * and then creates a query URI string to send to mod_pagespeed_statistics_json
 * using the given parameters.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {number} graphIndex The index of the graph queried.
 * @param {number} startTime The starting time of the data requested.
 * @param {number} endTime The ending time of the data requested.
 * @param {number} granularityMs The frequency of the datapoints requested.
 * @return {string} The URI to append to the query.
 */
pagespeed.MpsConsole.prototype.createQueryURI =
    function(isHistogram, graphIndex, startTime, endTime, granularityMs) {
  // TODO(bvb,sarahdw): Figure out nicer way of doing this.
  var queryString = '/mod_pagespeed_statistics?json';
  queryString += '&start_time=' + startTime;
  queryString += '&end_time=' + endTime;
  queryString += '&granularity=' + granularityMs;

  if (!isHistogram) {
    var var_data_needed = this.LineGraphs_[graphIndex].variables;
    queryString += '&var_titles=';
    for (var i = 0; i < var_data_needed.length; i++) {
      queryString += var_data_needed[i] + ',';
    }
  } else {
    queryString += '&hist_titles=';
    queryString += this.Histograms_[graphIndex].variable + ',';
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
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (this.readyState != 4) {
        return;
      }
      if (this.status != 200) {
        document.getElementById('error').style.display = 'block';
        return;
      }
      document.getElementById('error').style.display = 'none';
      document.getElementById('messages').innerHTML = this.responseText;
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
 * from an AJAX call to /mod_pagespeed_statistics_json. It then processes
 * the data and calls helper functions to redraw the graphs.
 * @param {pagespeed.MpsConsole.JSONData} data The data, parsed from the JSON
 *     response.
 * @param {number} graphIndex The graph queried.
 * @param {boolean} isHistogram Whether the graph is a histogram.
 * @param {number} endTime The ending time of the data requested, displayed if
 *   the user has queried a histogram.
 */
pagespeed.MpsConsole.prototype.scrapeData =
    function(data, graphIndex, isHistogram, endTime) {
  if (isHistogram) {
    this.updateHistogram(data['histograms'], graphIndex, endTime);
  } else {
    var timeSeriesData = this.computeTimeSeries(data['variables'],
                                                data['timestamps'],
                                                graphIndex);
   this.updateLineGraph(timeSeriesData, data['timestamps'], graphIndex);
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
  var options = {
    width: 900,
      height: (arrayOfBarsInfo.length + 1) * 25,
    legend: {position: 'none'},
    chartArea: {
      height: (arrayOfBarsInfo.length + 1) * 15,
      left: 200,
      top: 20
    }
  };
  for (var h = 0; h < arrayOfBarsInfo.length; h++) {
    dt.addRow([
      '[' + arrayOfBarsInfo[h.toString()].lowerBound + ', ' +
          arrayOfBarsInfo[h.toString()].upperBound + ')',
      arrayOfBarsInfo[h.toString()].count
    ]);
  }
  if (dt.getNumberOfRows()) {
    this.getBarGraph(graphTitle, time).draw(dt, options);
  }
};

/**
 * updateLineGraph takes new data and adds it to the
 * appropriate DataTable. It then draws/redraws the line graph to
 * include the new data.
 * @param {Array.<number>} varData The data to update the
 *     line graphs with.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A list of timestamps
 * representing when the data was logged.
 * @param {number} graphIndex The index of the line graph queried.
 */
pagespeed.MpsConsole.prototype.updateLineGraph =
    function(varData, timestamps, graphIndex) {
  // TODO(sarahdw, bvb): Better way of doing options that is shared between
  // histograms and line graphs.
  var graphTitle = this.LineGraphs_[graphIndex].title;
  var options = {
    width: 900,
    colors: ['#FF8000'],
    legend: {position: 'none'},
    hAxis: {
      format: 'MMM d, y hh:mm a'
    },
    vAxis: {
      minValue: 0,
      viewWindowMode: 'explicit',
      viewWindow: {
        min: 0
      }
    },
    chartArea: {
      left: 50,
      top: 20,
      width: 800
    },
    pointSize: 7
  };
  var dt = this.createLineGraphDataTable(graphTitle);
  for (var i = 0; i < timestamps.length; i++) {
    dt.addRow([new Date(parseInt(timestamps[i], 10)), varData[i]]);
  }
  if (dt.getNumberOfRows()) {
    this.getLineGraph(graphTitle).draw(dt, options);
  } else {
    this.displayNoDataFound(graphTitle);
  }
};

/**
 * displayNoDataFound displays a message on the console stating that no data
 * matched the user's query.
 * @param {string} graphTitle The title of the graph queried.
 */
pagespeed.MpsConsole.prototype.displayNoDataFound = function(graphTitle) {
  var noMatch = document.createElement('div');
  noMatch.setAttribute('class', 'no-match');
  noMatch.innerHTML = 'No data matches your query for ' + graphTitle;
  document.getElementById('container').appendChild(noMatch);
  setTimeout(function() {noMatch.parentNode.removeChild(noMatch);}, 4000);
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
  // TODO(sarahdw, bvb): Better way of getting the display title.
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
  var selectorButton = document.createElement('span');
  selectorButton.setAttribute('class', 'graphselector menuButton');
  this.createGraphDropdownOptions(wholeDiv, title, topBar, selectorButton);
  topBar.appendChild(graphTitle);
  topBar.appendChild(selectorButton);
  wholeDiv.appendChild(topBar);
};

/**
 * Creates the edit and remove options for a graph.
 * @param {Element} wholeDiv The div that the graph and title are drawn in.
 * @param {string} title The title of the graph.
 * @param {Element} topBar The div in which the selector and title are drawn.
 * @param {Element} selectorButton The button that displays the dropdown menu.
 */
pagespeed.MpsConsole.prototype.createGraphDropdownOptions =
    function(wholeDiv, title, topBar, selectorButton) {
  var menuDiv = document.createElement('div');
  menuDiv.setAttribute('class', 'goog-menu');
  menuDiv.style.display = 'none';
  if (!this.isHistogram(title)) {
    var edit = this.createEditGraphButton(title);
    menuDiv.appendChild(edit);
  }
  var removeButton = this.createRemoveGraphButton(wholeDiv, title);
  menuDiv.appendChild(removeButton);
  topBar.appendChild(menuDiv);
  var pm = new goog.ui.PopupMenu();
  pm.setToggleMode(true);
  pm.decorate(menuDiv);
  pm.attach(selectorButton,
            goog.positioning.Corner.BOTTOM_LEFT,
            goog.positioning.Corner.TOP_LEFT);
};

/**
 * createRemoveGraphButton creates the link that deletes a graph.
 * @param {Element} div The div in which the graph is drawn.
 * @param {string} title The name of the type of graph.
 * @return {Element} The element containing the remove button option.
 */
pagespeed.MpsConsole.prototype.createRemoveGraphButton =
    function(div, title) {
  var removeButton = document.createElement('div');
  removeButton.setAttribute('class', 'removeButton goog-menuitem');
  removeButton.innerHTML = 'Remove ' + title + ' Graph';
  removeButton.onclick = function() {
    div.parentNode.removeChild(div);
    removeButton.parentNode.removeChild(removeButton);
  };
  return removeButton;
};

/**
 * createEditGraphButton creates the link that edits a graph.
 * @param {string} title The name of the type of graph.
 * @return {Element} The element containing the edit button option.
 */
pagespeed.MpsConsole.prototype.createEditGraphButton =
    function(title) {
  var mpsconsole = this;
  var editButton = document.createElement('div');
  editButton.setAttribute('class', 'editButton goog-menuitem unimplemented');
  editButton.innerHTML = 'Edit ' + title + ' Graph';
  editButton.onclick = function() {
    // TODO(sarahdw, bvb): Functionality for edit graph button.
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
