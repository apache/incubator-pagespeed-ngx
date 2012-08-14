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

/**
 * @constructor
 */
pagespeed.MpsConsole = function() {
  /**
   * The data tables behind the charts (from the Charts API).
   * @type {Object.<string, Object.<string, (number|Date)>>}
   * @private
   */
  this.dataTables_ = {};
  /**
   * The data calculated to be displayed in the line graphs.
   * This is saved so that the each datapoint does not have to be
   * recalculated every time new data is received.
   * @type {Object.<string, Array.<number>>}
   * @private
   */
  this.varGraphData_ = {};
  /**
   * A list of timestamps of the data that is currently being displayed.
   * This is saved so that datapoints do not have to be recalculated.
   * @type {Array.<number>}
   * @private
   */
  this.timestamps_ = [];
  /**
   * The Charts API objects representing each of the graphs.
   * @type {Object.<string, Object>}
   * @private
   */
  this.graphs_ = {};
  /**
   * The titles of the line graphs.
   * @enum {string}
   * @private
   */
  this.LineGraphNames_ = {
    PERCENT_CACHE_HITS: 'Percent Cache Hits',
    QUERIES_PER_SECOND: 'Queries per Second',
    FLUSHES_PER_SECOND: 'Flushes per Second',
    FALLBACK_RESPONSES: 'Number of Fallback Responses Served per Second',
    SERF_BYTES_PER_REQUEST: 'Serf Bytes Fetched per Request',
    AVE_LOAD_TIME_MS: 'Average Load Time (ms)',
    REWRITES_EXEC_PER_SECOND: 'Rewrites Executed per Second',
    REWRITES_DROPPED_PER_SECOND: 'Rewrites Dropped per Second',
    RESOURCE_404S_PER_SECOND: 'Resource 404s per Second',
    SLURP_404S_PER_SECOND: 'Slurp 404s per Second',
    ONGOING_IMG_REWRITES: 'Ongoing Image Rewrites'
  };
  /**
   * Whether we're currently processing data. If so, calls to scrapeData should
   * simply return.
   * @type {boolean}
   * @private
   */
  this.loadingData_ = false;
  /** The size of the time window we want to show, in seconds
   * @type {number}
   * @const
   */
  this.TIME_WINDOW_SEC = 30;
  /**
   * The time between graph redraws, in milliseconds.
   * @type {number}
   * @const
   */
  this.UPDATE_INTERVAL_MS = 3000;
};

/**
 * Runs the console.
 * @return {pagespeed.MpsConsole} The console object.
 */
pagespeed.initConsole = function() {
  var console = new pagespeed.MpsConsole();
  console.createDivs();
  // TODO(sarahdw, bvb): Get rid of hardcoding
  var var_titles = ['num_flushes', 'cache_hits', 'cache_misses',
      'num_fallback_responses_served', 'slurp_404_count', 'page_load_count',
      'total_page_load_ms', 'num_rewrites_executed', 'num_rewrites_dropped',
      'resource_404_count', 'serf_fetch_request_count',
      'serf_fetch_bytes_count', 'image_ongoing_rewrites'];
  var hist_titles = ['Html Time us Histogram', 'Rewrite Latency Histogram',
      'Pagespeed Resource Latency Histogram',
      'Backend Fetch First Byte Latency Histogram'];
  // The start and end times are currently arbitrary values that will be valid
  // from July 2012 to a long time in the future.
  // TODO(sarahdw, bvb): unhardcode these arbitrary values
  var startTime = 1342724704685;
  var endTime = 1842824799999;
  var granularity_s = 5;
  console.initVarGraphData();
  console.getGraphs(var_titles, hist_titles, startTime, endTime, granularity_s);
  setInterval(function() {
    console.getGraphs(var_titles, hist_titles, startTime,
                      endTime, granularity_s);
  }, console.UPDATE_INTERVAL_MS);
  return console;
};

// Export this so the compiler doesn't rename it.
pagespeed['initConsole'] = pagespeed.initConsole;

/** @typedef {Object.<string,
      (Array.<pagespeed.MpsConsole.varData|
       pagespeed.MpsConsole.histogramData|
       pagespeed.MpsConsole.timestampData>)>} */
pagespeed.MpsConsole.JSONData;

/** @typedef {Object.<string, (number|Date)>} */
pagespeed.MpsConsole.varData;

/** @typedef {Array.<number>} */
pagespeed.MpsConsole.timestampData;

/** @typedef {Object.<string, (Array.<Object.<string, number>>)>} */
pagespeed.MpsConsole.histogramData;

/**
 * createDivs creates the layout of the console page.
 */
pagespeed.MpsConsole.prototype.createDivs = function() {
  var links = document.createElement('div');
  links.setAttribute('id', 'links');
  var error = document.createElement('div');
  error.setAttribute('id', 'error');
  error.innerHTML = 'There was an error updating the data.';
  var container = document.createElement('div');
  container.setAttribute('id', 'container');
  var sidebar = document.createElement('div');
  sidebar.setAttribute('id', 'sidebar');
  document.body.appendChild(sidebar);

  var toggleErrorMessages = document.createElement('button');
  toggleErrorMessages.innerHTML = 'Show Recent Errors';
  toggleErrorMessages.id = 'toggleErrorMessages';
  toggleErrorMessages.onclick = this.loadMessagesData;

  this.createAddGraphButton();
  sidebar.appendChild(document.createElement('br'));
  sidebar.appendChild(toggleErrorMessages);
  container.appendChild(links);
  container.appendChild(document.createElement('br'));
  container.appendChild(document.createElement('br'));
  document.body.appendChild(error);
  document.body.appendChild(container);
};

/**
 * Initializes each line graph's data object to be an empty array.
 */
pagespeed.MpsConsole.prototype.initVarGraphData = function() {
  var names = this.LineGraphNames_;
  for (var n in names) {
    if (names.hasOwnProperty(n)) {
      this.varGraphData_[names[n]] = [];
    }
  }
};

/**
 * getGraphs issues an XHR to request data from the server for
 * the graphs with the given parameters.
 * @param {Array.<string>} var_titles The names of the variables queried.
 * @param {Array.<string>} hist_titles The titles of the histograms queried.
 * @param {number} startTime The starting time of the data requested.
 * @param {number} endTime The ending time of the data requested.
 * @param {number} granularity The frequency of the datapoints requested.
 */
pagespeed.MpsConsole.prototype.getGraphs =
    function(var_titles, hist_titles, startTime, endTime, granularity) {
  var xhr = new XMLHttpRequest();
  var console = this;
  xhr.onreadystatechange = function() {
    if (this.readyState != 4) {
      return;
    }
    if (this.status != 200) {
      document.getElementById('error').style.display = 'block';
      return;
    }
    document.getElementById('error').style.display = 'none';
    console.scrapeData(JSON.parse(this.responseText));
  };

  // TODO(bvb,sarahdw): Figure out nicer way of doing this.
  // TODO(bvb,sarahdw): Detect when server returns 'statistics not enabled'.
  var queryString = '?start_time=' + startTime;
  queryString += '&end_time=' + endTime;
  queryString += '&hist_titles=';
  for (var i = 0; i < hist_titles.length; i++) {
    queryString = queryString + hist_titles[i] + ',';
  }
  queryString += '&var_titles=';
  for (var i = 0; i < var_titles.length; i++) {
    queryString = queryString + var_titles[i] + ',';
  }

  queryString += '&granularity=' + granularity;
  xhr.open('GET', '/mod_pagespeed_statistics_json' + queryString);
  xhr.send();
};

/**
 * scrapeData scrapes data out of the text passed to it, which is presumed to be
 * from an AJAX call to /mod_pagespeed_statistics_json. It then processes
 * the data and calls helper functions to redraw the graphs.
 * @param {pagespeed.MpsConsole.JSONData} data The data from the JSON response.
 */
pagespeed.MpsConsole.prototype.scrapeData = function(data) {
  if (this.loadingData_) {
    return;
  }
  this.loadingData_ = true;
  var timeSeriesData =
      this.computeTimeSeries(data['variables'], data['timestamps']);
  this.updateAllLineGraphs(timeSeriesData, data['timestamps']);
  this.updateAllHistograms(data['histograms']);
  this.loadingData_ = false;
};

/**
 * computeTimeSeries goes through the mod_pagespeed_statistics variables scraped
 * by scrapeData and calculates useful values.
 * Some values are calculated by diffing two consecutive data entries.
 * @param {pagespeed.MpsConsole.varData} data The scraped variables.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A list of timestamps.
 * @return {pagespeed.MpsConsole.varData} The calculated values.
 */
pagespeed.MpsConsole.prototype.computeTimeSeries = function(data, timestamps) {
  var names = this.LineGraphNames_;
  for (var i = this.timestamps_.length; i < timestamps.length; i++) {
    if (i == 0) {
      continue;
    }
    var previous_timestamp_ms = timestamps[i - 1];
    var current_timestamp_ms = timestamps[i];

    var totalCacheContacts = data['cache_hits'][current_timestamp_ms] +
        data['cache_misses'][current_timestamp_ms] -
        data['cache_hits'][previous_timestamp_ms] -
        data['cache_misses'][previous_timestamp_ms];
    this.varGraphData_[names.PERCENT_CACHE_HITS][i - 1] =
        this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                       'cache_hits', totalCacheContacts);
    var pageLoads = data['page_load_count'][current_timestamp_ms] -
        data['page_load_count'][previous_timestamp_ms];
    this.varGraphData_[names.AVE_LOAD_TIME_MS][i - 1] =
        this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                       'total_page_load_ms', pageLoads);
    var serfRequests = data['serf_fetch_request_count'][current_timestamp_ms] -
        data['serf_fetch_request_count'][previous_timestamp_ms];
    this.varGraphData_[names.SERF_BYTES_PER_REQUEST][i - 1] =
        this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
                       'serf_fetch_bytes_count', serfRequests);

    this.varGraphData_[names.QUERIES_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms, 'page_load_count');
    this.varGraphData_[names.REWRITES_EXEC_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms,
                                  'num_rewrites_executed');
    this.varGraphData_[names.REWRITES_DROPPED_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms,
                                  'num_rewrites_dropped');
    this.varGraphData_[names.RESOURCE_404S_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms, 'resource_404_count');
    this.varGraphData_[names.SLURP_404S_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms, 'slurp_404_count');
    this.varGraphData_[names.FLUSHES_PER_SECOND][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms, 'num_flushes');
    this.varGraphData_[names.FALLBACK_RESPONSES][i - 1] =
        this.ratioStatElapsedTime(data, current_timestamp_ms,
                                  previous_timestamp_ms,
                                  'num_fallback_responses_served');

    this.varGraphData_[names.ONGOING_IMG_REWRITES][i - 1] =
        data['image_ongoing_rewrites'][current_timestamp_ms];
  }
  this.timestamps_ = timestamps;
  return this.varGraphData_;
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
 * @return {number} The computed value.
 */
pagespeed.MpsConsole.prototype.ratioStatElapsedTime =
    function(data, current_timestamp_ms, previous_timestamp_ms, property) {
  return this.ratioStat(data, current_timestamp_ms, previous_timestamp_ms,
      property, (current_timestamp_ms - previous_timestamp_ms) / 1000);
};

/**
 * updateAllHistograms adds new data to the appropriate
 * DataTable. It then redraws each histogram to
 * include the new data.
 * @param {pagespeed.MpsConsole.histogramData}
 *     histData The parsed histogram data.
 */
pagespeed.MpsConsole.prototype.updateAllHistograms = function(histData) {
  for (var value in histData) {
    if (histData.hasOwnProperty(value)) {
      // Skip the prototype properties.
      var dt = this.getHistogramDataTable(value);
      // TODO(sarahdw, bvb): Do we really need to delete all the rows and redraw
      // them? It seems inefficient.
      dt.removeRows(0, dt.getNumberOfRows());
      var arrayOfBarsInfo = histData[value];
      var options = {
        title: value,
        width: 1000,
        height: 30 * arrayOfBarsInfo.length,
        legend: {position: 'none'}
      };
      for (var h = 0; h < arrayOfBarsInfo.length; h++) {
        dt.addRow([
          '[' + arrayOfBarsInfo[h.toString()].lowerBound + ', ' +
              arrayOfBarsInfo[h.toString()].upperBound + ')',
          arrayOfBarsInfo[h.toString()].count
        ]);
      }
      if (dt.getNumberOfRows()) {
        this.getBarGraph(value).draw(dt, options);
      }
    }
  }
};

/**
 * updateAllLineGraphs takes new data and adds it to the
 * appropriate DataTable. It then redraws each line graph to
 * include the new data.
 * @param {Object.<string, (Array.<number>)>} varData The data to update the
 *     line graphs with.
 * @param {pagespeed.MpsConsole.timestampData} timestamps A
 *     list of timestamps representing when the data was logged.
 */
pagespeed.MpsConsole.prototype.updateAllLineGraphs =
    function(varData, timestamps) {
  for (var value in varData) {
    // Skip the prototype properties.
    if (varData.hasOwnProperty(value)) {
      var options = {
        title: unescape(value),
        width: 1000,
        height: 275,
        legend: {position: 'none'},
        hAxis: {
          title: 'time',
          format: 'MMM d, y hh:mm:ss a'
        },
        vAxis: {
          minValue: 0,
          viewWindowMode: 'explicit',
          viewWindow: {
            min: 0
          }
        }
      };
      var dt = this.getLineGraphDataTable(value);
      // TODO(sarahdw, bvb): Remove unnecessary row removal and redraw.
      dt.removeRows(0, dt.getNumberOfRows());
      // Because some values require a difference calculation, we actually
      // calculate one fewer row than there are timestamps.
      for (var i = 0; i < timestamps.length - 1; i++) {
        dt.addRow([new Date(parseInt(timestamps[i], 10)), varData[value][i]]);
      }
      this.getLineGraph(value).draw(dt, options);
    }
  }
};

/**
 * createGraphDiv creates the necessary DOM elements for a graph, and returns
 * the div in which the graph will be drawn.
 * @param {string} title The title of the graph.
 * @return {Element} the div in which to draw the graph.
 */
pagespeed.MpsConsole.prototype.createGraphDiv = function(title) {
  var graph = document.createElement('div');
  graph.id = title;
  graph.setAttribute('class', 'graph');
  graph.style.display = 'none';
  document.getElementById('container').appendChild(graph);
  var topLink = document.createElement('a');
  if (document.getElementById('links').childNodes.length) {
    topLink.setAttribute('class', 'unselected');
    var links = document.getElementById('links');
    if (links.childNodes.length % 8 == 0) {
    // TODO(sarahdw): Better navigation.
      links.appendChild(document.createElement('br'));
      links.appendChild(document.createElement('br'));
    }
  } else {
    topLink.id = 'selected';
    document.getElementById(title).style.display = 'block';
  }
  topLink.onclick = function() {
     if (document.getElementById('selected')) {
       document.getElementById('selected').setAttribute('class', 'unselected');
       document.getElementById('selected').id = '';
     }
     topLink.id = 'selected';
     var graphs = document.getElementsByClassName('graph');
     for (var i = 0; i < graphs.length; i++) {
       graphs[i].style.display = 'none';
     }
     var buttons = document.getElementsByClassName('removeButton');
     for (var i = 0; i < buttons.length; i++) {
       buttons[i].parentNode.removeChild(buttons[i]);
     }
     document.getElementById(title).style.display = 'block';
  }
  topLink.innerHTML = unescape(title);
  document.getElementById('links').appendChild(topLink);
  return graph;
};

/**
 * createAddGraphButton gives the user the ability to display another
 * graph on the page. A dropdown menu is created so that the user can
 * select which other graphs to display.
 */
pagespeed.MpsConsole.prototype.createAddGraphButton = function() {
  document.getElementById('sidebar').appendChild(document.createElement('br'));
  var graphList = document.createElement('select');
  graphList.id = 'graphList';
  var addGraph = document.createElement('button');
  addGraph.innerHTML = 'add to page';
  addGraph.onclick = function() {
    var selected = graphList.options[graphList.selectedIndex];
    var graph = document.getElementById(selected.value);
    if (graph.style.display == 'block') return;
    graph.style.display = 'block';
    var remove = document.createElement('button');
    remove.setAttribute('class', 'removeButton');
    remove.innerHTML = 'Remove ' + unescape(selected.value) + ' Graph';
    remove.onclick = function() {
      graph.style.display = 'none';
      remove.parentNode.removeChild(remove);
    };
    graph.parentNode.insertBefore(remove, graph.nextSibling);
  };
  document.getElementById('sidebar').appendChild(graphList);
  document.getElementById('sidebar').appendChild(addGraph);
};

/**
 * loadMessagesData sends an AJAX request to scrape the
 * error messages at /mod_pagespeed_message. It then displays the error messages
 * in the appropriate div, creating it if necessary.
 */
pagespeed.MpsConsole.prototype.loadMessagesData = function() {
  var errorMessages = document.getElementById('errorMessages');
  var toggleErrorMessages = document.getElementById('toggleErrorMessages');
  if (!errorMessages) {
    errorMessages = document.createElement('div');
    errorMessages.id = 'errorMessages';
    errorMessages.style.display = 'none';
    document.getElementById('container').appendChild(errorMessages);
  }
  if (errorMessages.style.display == 'none') {
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
      errorMessages.innerHTML = this.responseText;
      errorMessages.style.display = 'block';
      toggleErrorMessages.innerHTML = 'Hide Recent Errors';
    };

    xhr.open('GET', '/mod_pagespeed_message');
    xhr.send();
  } else {
    errorMessages.style.display = 'none';
    toggleErrorMessages.innerHTML = 'Show Recent Errors';
  }
};

/**
 * getLineGraph creates a new line graph and the div in which it is
 * displayed. If the graph already exists, it returns that graph.
 * @param {string} title The title of the graph.
 * @return {Object} The graph.
 */
pagespeed.MpsConsole.prototype.getLineGraph = function(title) {
  var g = this.graphs_[title];
  if (!g) {
    g = new google.visualization.LineChart(this.createGraphDiv(title));
    this.graphs_[title] = g;
    this.addOption(title);
  }
  return g;
};

/**
 * addOption adds the given graph to the list of graphs
 * in the dropdown menu.
 * @param {string} title The name of the newly added graph.
 */
pagespeed.MpsConsole.prototype.addOption = function(title) {
  var option = document.createElement('option');
  option.value = title;
  option.innerHTML = unescape(title);
  document.getElementById('graphList').add(option, null);
};

/**
 * getBarGraph creates a new bar graph and the div in which it is
 * displayed. If the graph already exists, it returns that graph.
 * @param {string} title The title of the graph.
 * @return {Object} The graph.
 */
pagespeed.MpsConsole.prototype.getBarGraph = function(title) {
  var g = this.graphs_[title];
  if (!g) {
    g = new google.visualization.BarChart(this.createGraphDiv(title));
    this.graphs_[title] = g;
    this.addOption(title);
  }
  return g;
};

/**
 * getLineGraphDataTable adds a new google.
 * visualization.DataTable to the array of DataTables and returns it.
 * Each DataTable has two columns: a timestamp (represented as a number, time
 * elapsed in s) and the value at that time, also a number.
 * This DataTable is meant for a line graph DataView.
 * If the table already exists, it returns that table.
 * @param {string} title The name of the variable being measured.
 * @return {Object} The data table.
 */
pagespeed.MpsConsole.prototype.getLineGraphDataTable = function(title) {
  var dt = this.dataTables_[title];
  if (!dt) {
    dt = new google.visualization.DataTable();
    dt.addColumn('datetime', 'Time');
    dt.addColumn('number', title);
    this.dataTables_[title] = dt;
  }
  return dt;
};

/**
 * getHistogramDataTable adds a new
 * google.visualization.DataTable to the array of DataTables and
 * returns it.
 * Each DataTable has two columns: a string and a number.
 * This DataTable is meant for a bar graph DataView.
 * If the table already exists, it returns that table.
 * @param {string} title The name of the variable being measured.
 * @return {Object} The data table.
 */
pagespeed.MpsConsole.prototype.getHistogramDataTable = function(title) {
  var dt = this.dataTables_[title];
  if (!dt) {
    dt = new google.visualization.DataTable();
    dt.addColumn('string', 'Bounds');
    dt.addColumn('number', title);
    this.dataTables_[title] = dt;
  }
  return dt;
};
