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
   * The time the first scrape occurred, used to calculate elapsed time.
   * @type {Date}
   * @private
   */
  this.firstScrapeDate_ = null;
  /**
   * The data tables behind the charts (from the Charts API).
   * @type {Object.<string, Object.<string, (number|Date)>>}
   * @private
   */
  this.dataTables_ = {};
  /**
   * The data from the currently-active scrape.
   * @type {Object.<string, (number|Date)>}
   * @private
   */
  this.currData_ = null;
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
  console.loadData();
  // We need at least two data points to begin plotting, so we include this
  // setTimeout to obtain them more quickly than within the update interval.
  setTimeout(function() {
    console.loadData();
  }, 100);
  setInterval(function() {
    console.loadData();
  }, console.UPDATE_INTERVAL_MS);
  return console;
};

// Export this so the compiler doesn't rename it.
pagespeed['initConsole'] = pagespeed.initConsole;

/**
 * createDivs adds the necessary divs to the console page.
 */
pagespeed.MpsConsole.prototype.createDivs = function() {
  var links = document.createElement('div');
  links.setAttribute('id', 'links');
  var error = document.createElement('div');
  error.setAttribute('id', 'error');
  error.innerHTML = 'There was an error updating the data.';
  var container = document.createElement('div');
  container.setAttribute('id', 'container');
  document.body.appendChild(links);
  document.body.appendChild(error);
  document.body.appendChild(container);
};

/**
 * loadData issues an XHR to get the contents of /mod_pagespeed_statistics
 * and then passes the result to scrapeData for processing.
 */
pagespeed.MpsConsole.prototype.loadData = function() {
  var xhr = new XMLHttpRequest();
  var self = this;
  xhr.onreadystatechange = function() {
    if (this.readyState != 4) {
      return;
    }
    if (this.status != 200) {
      document.getElementById('error').style.display = 'block';
      return;
    }
    document.getElementById('error').style.display = 'none';
    self.scrapeData(this.responseText);
  };
  xhr.open('GET', '/mod_pagespeed_statistics');
  xhr.send();
};

/**
* scrapeData scrapes data out of the text passed to it, which is presumed to be
* from an AJAX call to /mod_pagespeed_statistics. It then calls a helper to
* redraw the graphs.
* @param {string} text The text from which data should be scraped.
*/
pagespeed.MpsConsole.prototype.scrapeData = function(text) {
  if (this.loadingData_) {
    return;
  }
  this.loadingData_ = true;
  var data = {};
  data['time'] = new Date();
  if (this.firstScrapeDate_ == null) {
    this.firstScrapeDate_ = data['time'];
  }
  this.parseValues(text, data);
  var timeSeriesData = this.computeTimeSeries(this.currData_, data);
  this.currData_ = data;
  var histData = this.parseHistogramStats(text);
  this.updateAllGraphs(timeSeriesData, histData);
  this.loadingData_ = false;
};

/**
 * parseValues scrapes the given text to create a mapping of string keys to
 * integer values.
 * @param {string} text The text to parse.
 * @param {Object.<string, (number|Date)>} data The data object that
 *     parsed values will be added to.
 */
pagespeed.MpsConsole.prototype.parseValues = function(text, data) {
  // Match 'value:     ###'.
  var pattern = /([A-Za-z_0-9]+):\s+(-?\d+)/g;
  for (var result = pattern.exec(text); result; result = pattern.exec(text)) {
    // Repeated calls to exec() find the next match.
    var name = result[1];
    var value = parseInt(result[2], 10);
    data[name] = value;
  }
};

/**
 * computeTimeSeries goes through the mod_pagespeed_statistics variables scraped
 * by scrapeData and calculates useful values. It then calls
 * updateAllGraphs to display the data.
 * Some values are calculated by diffing the previous scrape with the
 * current scrape, hence old and newData.
 * @param {Object.<string, (number|Date)>} oldData The old scraped variables.
 * @param {Object.<string, (number|Date)>} newData The new scraped variables.
 * @return {Object.<string, (number|Date)>} The parsed data.
 */
pagespeed.MpsConsole.prototype.computeTimeSeries = function(oldData, newData) {
  var parsedData = {};
  parsedData['time'] = newData['time'];
  var names = this.LineGraphNames_;
  if (oldData) {
    // TODO(bvb, sarahdw): Note somewhere in UI that we plot average since last
    // scrape versus average over all time.
    var elapsedTimeSec = (newData['time'] - oldData['time']) / 1000;
    var totalCacheContacts = newData['cache_hits'] + newData['cache_misses'] -
        oldData['cache_hits'] - oldData['cache_misses'];
    parsedData[names.PERCENT_CACHE_HITS] =
        this.ratioStat(oldData, newData, 'cache_hits', totalCacheContacts);
    var pageLoads = newData['page_load_count'] - oldData['page_load_count'];
    parsedData[names.AVE_LOAD_TIME_MS] =
        this.ratioStat(oldData, newData, 'total_page_load_ms', pageLoads);
    parsedData[names.QUERIES_PER_SECOND] =
        this.ratioStat(oldData, newData, 'page_load_count', elapsedTimeSec);
    parsedData[names.REWRITES_EXEC_PER_SECOND] =
        this.ratioStat(oldData, newData,
                       'num_rewrites_executed', elapsedTimeSec);
    parsedData[names.REWRITES_DROPPED_PER_SECOND] =
        this.ratioStat(oldData, newData,
                       'num_rewrites_dropped', elapsedTimeSec);
    parsedData[names.RESOURCE_404S_PER_SECOND] =
        this.ratioStat(oldData, newData, 'resource_404_count', elapsedTimeSec);
    parsedData[names.SLURP_404S_PER_SECOND] =
        this.ratioStat(oldData, newData, 'slurp_404_count', elapsedTimeSec);
    parsedData[names.FLUSHES_PER_SECOND] =
        this.ratioStat(oldData, newData, 'num_flushes', elapsedTimeSec);
    parsedData[names.FALLBACK_RESPONSES] =
        this.ratioStat(oldData, newData,
                       'num_fallback_responses_served', elapsedTimeSec);
    var serfRequests = newData['serf_fetch_request_count'] -
        oldData['serf_fetch_request_count'];
    parsedData[names.SERF_BYTES_PER_REQUEST] =
        this.ratioStat(oldData, newData,
                       'serf_fetch_bytes_count', serfRequests);
  } else {
    // On the first run, there won't be any oldData yet.
    for (var n in names) {
      // We're not interested in names's prototype's values
      if (!names.hasOwnProperty(n)) continue;
      parsedData[names[n]] = 0;
    }
  }
  // We don't need oldData for these.
  parsedData[names.ONGOING_IMG_REWRITES] = newData['image_ongoing_rewrites'];

  return parsedData;
};

/**
 * ratioStat calculates the ratio of two values over some common denominator.
 * It prevents NaN errors by returning 0 if the denominator is 0.
 * @param {Object.<string, (number|Date)>} oldData The old scraped variables.
 * @param {Object.<string, (number|Date)>} newData The new scraped variables.
 * @param {string} property The parameter to compare across the two datasets.
 * @param {number} denominator The value by which to divide the difference.
 * @return {number} The computed value.
 */
pagespeed.MpsConsole.prototype.ratioStat =
    function(oldData, newData, property, denominator) {
  if (denominator == 0) {
    return 0;
  } else {
    return (newData[property] - oldData[property]) / denominator;
  }
};

/**
 * parseHistogramStats parses general statistics from the histograms
 * in the given text.
 * @param {string} text The text to find statistics in.
 * @return {Object.<string,
 *     Object.<string, (number|Array.<Object.<string, number>>)>>}
 *     The scraped histogram data.
 */
pagespeed.MpsConsole.prototype.parseHistogramStats = function(text) {
  // Match a <td> within the histogram representation.
  function tdPattern(text) {
    return '<td style\\\=\\\"?text-align:right\\\;padding:[0.25em ]+\\\">' +
        text +
        '<\\\/td>';
    }
  // Match a bar within the histogram representation.
  var barPattern = '<tr>' +
      '<td style\\\=\\\"padding: 0 0 0 0.25em\\\">\\\[<\\\/td>' +
      tdPattern('(\\\d+),') + tdPattern('(\\\d+)\\\)') +
      tdPattern('(\\\d+)') + tdPattern('(\\\d+\\\.?\\\d*)%') +
      tdPattern('(\\\d+\\\.?\\\d*)%') +
      '<td><div style=\\\"width: \\\d+px;height:\\\d+px;background-color:' +
      'blue\\\"></div></td>';
  // Match a decimal number, e.g. ##.###.
  var decPattern = '(\\\d+(\\\.\\\d)?)';
  // Match a histogram as produced by /mod_pagespeed_statistics
  var histogramPattern = new RegExp(
      '<h3>([A-Za-z ]+)<\\\/h3>' +
      '<div style\\\=\\\'float\\\:left\\\;\\\'><\\\/div>' +
      '<div><span style\\\=\\\'cursor\\\:pointer\\\;\\\' onclick\\\=' +
      '\\\"toggleVisible\\\(\\\'id[a-zA-Z0-9]+\\\'\\\)\\\">\\\&gt\\\;' +
      'Raw Histogram Data\\\.\\\.\\\.<\\\/span><div id\\\=\\\'id[a-zA-Z0-9]+' +
      '\\\' style\\\=\\\'display\\\:none\\\;\\\'><hr\\\/>' +
      'Count: ' + decPattern + ' \\\| Avg: ' + decPattern + ' \\\| ' +
      'StdDev: ' + decPattern + ' \\\| Min: ' + decPattern + ' \\\| ' +
      'Median: ' + decPattern + ' \\\| Max: ' + decPattern + ' \\\| ' +
      '90%: ' + decPattern + ' \\\| 95%: ' + decPattern + ' \\\| ' +
      '99%: ' + decPattern + '<hr><table>' +
      '(' + barPattern + ')+<\\\/table>',
      'g');
  var stats = {};
  for (var result = histogramPattern.exec(text);
       result;
       result = histogramPattern.exec(text)) {
       // Repeated calls to exec() find the next match.
    var curr = {};
    // TODO(bvb,sarahdw): decide what info here is needed
    curr['title'] = result[1];
    curr['count'] = parseInt(result[2], 10);
    curr['avg'] = parseFloat(result[4]);
    curr['stddev'] = parseFloat(result[6]);
    curr['min'] = parseInt(result[8], 10);
    curr['median'] = parseInt(result[10], 10);
    curr['max'] = parseInt(result[12], 10);
    curr['90p'] = parseInt(result[14], 10);
    curr['95p'] = parseInt(result[16], 10);
    curr['99p'] = parseInt(result[18], 10);
    this.parseHistogramBars(result[0], new RegExp(barPattern, 'g'), curr);
    stats[result[1]] = curr;
  }
  return stats;
};

/**
 * parseHistogramBars parses the information from the bars present in
 * any histograms in the given text.
 * @param {string} text The text to find bars in.
 * @param {RegExp} barPattern The pattern to match bars with.
 * @param {Object.<string, (number|Array.<Object.<string, number>>)>}
 *     stats The object that will store the scraped histogram bars.
 */
pagespeed.MpsConsole.prototype.parseHistogramBars =
    function(text, barPattern, stats) {
  stats['bars'] = [];
  for (var barResult = barPattern.exec(text);
       barResult;
       barResult = barPattern.exec(text)) {
       // Repeated calls to exec() find the next match.
    stats['bars'].push({
      lowerBound: parseInt(barResult[1], 10),
      upperBound: parseInt(barResult[2], 10),
      count: parseInt(barResult[3], 10),
      partialPercent: parseFloat(barResult[4]),
      cumulativePercent: parseFloat(barResult[5])
    });
  }
};

/**
* updateAllGraphs updates the DataTables of and redraws all the graphs.
* @param {Object.<string, (number|Date)>} data The new set of data.
* @param {Object.<string, Object.<string, (number|Array.<Object.<string, number>>)>>}
*     histData The parsed histogram data.
*/
pagespeed.MpsConsole.prototype.updateAllGraphs = function(data, histData) {
  this.updateAllLineGraphs(data);
  this.updateAllHistograms(histData);
};

/**
* updateAllLineGraphs takes new data and adds it to the
* appropriate DataTable. It then redraws each line graph to
* include the new data.
* @param {Object.<string, (number|Date)>} data The data to update the
*     line graphs with.
*/
pagespeed.MpsConsole.prototype.updateAllLineGraphs = function(data) {
  var deltaT = (data.time - this.firstScrapeDate_) / 1000;
  for (var value in data) {
    if (!data.hasOwnProperty(value) || value == 'time') {
      // We're not interested in graphing the time or the prototype properties.
      continue;
    }
    var options = {
      title: value,
      height: 275,
      legend: {position: 'none'},
      hAxis: {title: 'elapsed time (s)'},
      vAxis: {
        minValue: 0,
        viewWindowMode: 'explicit',
        viewWindow: {
          min: 0
        }
      }
    };
    var dt = this.getLineGraphDataTable(value);
    dt.addRow([deltaT, data[value]]);
    if (dt.getNumberOfRows()) {
      // Update time window.
      var lowestTime = dt.getValue(dt.getNumberOfRows() - 1, 0) -
          this.TIME_WINDOW_SEC;
      var rowsToRemove = dt.getFilteredRows([{
        column: 0,
        minValue: 0,
        maxValue: lowestTime
      }]);
      for (var j = 0; j < rowsToRemove.length; ++j) {
        dt.removeRow(j);
      }
      this.getLineGraph(value).draw(dt, options);
    }
  }
};

/**
* updateAllHistograms adds new data to the appropriate
* DataTable. It then redraws each histogram to
* include the new data.
* @param {Object.<string, Object.<string, (number|Array.<Object.<string, number>>)>>}
*     histData The parsed histogram data.
*/
pagespeed.MpsConsole.prototype.updateAllHistograms = function(histData) {
  for (var value in histData) {
    if (histData.hasOwnProperty(value)) {
      // Skip the prototype properties.
      var dt = this.getHistogramDataTable(value);
      dt.removeRows(0, dt.getNumberOfRows());
      var arrayOfBarsInfo = histData[value]['bars'];
      if (!arrayOfBarsInfo) continue;
      var options = {
        title: value,
        height: 30 * arrayOfBarsInfo.length,
        legend: {position: 'none'}
      };
      for (var h = 0; h < arrayOfBarsInfo.length; h++) {
        dt.addRow([
          '[' + arrayOfBarsInfo[h].lowerBound + ', ' +
              arrayOfBarsInfo[h].upperBound + ')',
          arrayOfBarsInfo[h].count
        ]);
      }
      if (dt.getNumberOfRows()) {
        this.getBarGraph(value).draw(dt, options);
      }
    }
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
  }
  return g;
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
  }
  return g;
};

/**
 * createGraphDiv creates the necessary DOM elements for a graph, and returns
 * the div in which the graph will be drawn.
 * @param {string} title The title of the graph.
 * @return {Element} the div in which to draw the graph.
 */
pagespeed.MpsConsole.prototype.createGraphDiv = function(title) {
  var graph = document.createElement('div');
  var link = document.createElement('a');
  link.name = title;
  link.appendChild(graph);
  document.getElementById('container').appendChild(link);
  var topLink = document.createElement('a');
  topLink.href = '#' + title;
  topLink.innerHTML = title;
  document.getElementById('links').appendChild(topLink);
  document.getElementById('links').appendChild(document.createElement('br'));
  return graph;
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
    dt.addColumn('number', 'Time');
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
