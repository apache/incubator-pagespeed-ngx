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
 */

/**
 * @fileoverview Code for adding auto-refreshing bar charts and realtime
 * annotated time line on the basis of the data in statistics page.
 *
 * TODO(xqyin): Integrate this into console page.
 *
 * @author oschaaf@we-amp.com (Otto van der Schaaf)
 * @author xqyin@google.com (XiaoQian Yin)
 */

goog.provide('pagespeed.Graphs');

goog.require('goog.array');
goog.require('goog.events');
goog.require('goog.net.EventType');
goog.require('goog.net.XhrIo');
goog.require('goog.string');

// Google Charts API.
// Requires <script src='https://www.google.com/jsapi'></script> loaded in HTML.
google.load('visualization', '1',
            {'packages': ['table', 'corechart', 'annotatedtimeline']});


/** @typedef {{name: string, value:string}} */
pagespeed.StatsNode;


/** @typedef {{messages: Array.<pagespeed.StatsNode>, timeReceived: Date}} */
pagespeed.StatsArray;



/**
 * @constructor
 * @param {goog.testing.net.XhrIo=} opt_xhr Optional mock XmlHttpRequests
 *     handler for testing.
 */
pagespeed.Graphs = function(opt_xhr) {
  /**
   * The XmlHttpRequests handler for auto-refresh. We pass a mock XhrIo
   * when testing. Default is a normal XhrIo.
   * @private {goog.net.XhrIo|goog.testing.net.XhrIo}
   */
  this.xhr_ = opt_xhr || new goog.net.XhrIo();

  /**
   * This array of arrays collects historical data from statistics log file
   * for the first refresh and updates from back end to get snapshot of the
   * current data. The i-th entry of the array is the statistics corresponds
   * to the i-th time stamp, which is an array of special nodes. Each node
   * contains certain statistics name and the value. We use the last entry to
   * generate pie charts showing the most recent data. The whole array of
   * arrays is used to generate line charts showing statistics history. We only
   * keep one-day data to avoid infinite growth of this array.
   * @private {!Array.<pagespeed.StatsArray>}
   */
  this.psolMessages_ = [];

  /**
   * The option of auto-refresh. If true, the page will automatically refresh
   * itself every 5 seconds.
   * @private {boolean}
   */
  this.autoRefresh_ = false;
  // We set the default to false because auto-refresh would impact the user
  // experience of annotated time line.

  /**
   * The flag of whether the first refresh is started. We need to call a
   * refresh when loading the page for the initialization. Default is false,
   * which means the page hasn't started the first refresh yet.
   * @private {boolean}
   */
  this.firstRefreshStarted_ = false;

  /**
   * The flag of whether the second refresh is started. Since we fetch data
   * from statistics log file for the first refresh, the data may be stale.
   * We need to do the second refresh immediately to make sure the data we are
   * showing is up-to-date.
   * @private {boolean}
   */
  this.secondRefreshStarted_ = false;

  /**
   * Maps chart titles to the chart object.
   * @private {Object}
   */
  this.chartCache_ = {};

  // Hide all div elements.
  // To use AnnotatedTimeLine charts, we must specify the size of the container
  // elements explicitly instead of setting the size in the options of charts.
  // Since we cannot get the size of an element with 'display:none;', we hide
  // all the elements by positioning them off the left hand side.
  for (var i in pagespeed.Graphs.DisplayDiv) {
    var chartDiv = document.getElementById(pagespeed.Graphs.DisplayDiv[i]);
    chartDiv.className = 'pagespeed-hidden-offscreen';
  }

  // The navigation bar to switch among different display modes.
  var navElement = document.createElement('table');
  navElement.id = 'nav-bar';
  navElement.className = 'pagespeed-sub-tabs';
  navElement.innerHTML =
      '<tr><td><a id="' + pagespeed.Graphs.DisplayMode.CACHE_APPLIED +
      '" href="javascript:void(0);">' +
      'Per application cache stats</a> - </td>' +
      '<td><a id="' + pagespeed.Graphs.DisplayMode.CACHE_TYPE +
      '" href="javascript:void(0);">' +
      'Per type cache stats</a> - </td>' +
      '<td><a id="' + pagespeed.Graphs.DisplayMode.IPRO +
      '" href="javascript:void(0);">' +
      'IPRO status</a> - </td>' +
      '<td><a id="' + pagespeed.Graphs.DisplayMode.REWRITE_IMAGE +
      '" href="javascript:void(0);">' +
      'Image rewriting</a> - </td>' +
      '<td><a id="' + pagespeed.Graphs.DisplayMode.REALTIME +
      '" href="javascript:void(0);">' +
      'Realtime</a></td></tr>';
  // The UI table of auto-refresh.
  var uiTable = document.createElement('div');
  uiTable.id = 'ui-div';
  uiTable.innerHTML =
      '<table id="ui-table" border=1 style="border-collapse: ' +
      'collapse;border-color:silver;"><tr valign="center">' +
      '<td>Auto refresh (every 5 seconds): <input type="checkbox" ' +
      'id="auto-refresh" ' + (this.autoRefresh_ ? 'checked' : '') +
      '></td></tr></table>';
  document.body.insertBefore(
      uiTable,
      document.getElementById(pagespeed.Graphs.DisplayDiv.CACHE_APPLIED));
  document.body.insertBefore(navElement, document.getElementById('ui-div'));
};


/**
 * Show the chosen div element and hide all other elements.
 * @param {string} div The chosen div element to display.
 */
pagespeed.Graphs.prototype.show = function(div) {
  // Only shows the div chosen by users.
  var i;
  for (i in pagespeed.Graphs.DisplayDiv) {
    var chartDiv = pagespeed.Graphs.DisplayDiv[i];
    if (chartDiv == div) {
      document.getElementById(chartDiv).className = '';
    } else {
      document.getElementById(chartDiv).className =
          'pagespeed-hidden-offscreen';
    }
  }

  // Underline the current tab.
  var currentTab = document.getElementById(div + '_mode');
  for (i in pagespeed.Graphs.DisplayMode) {
    var link = document.getElementById(pagespeed.Graphs.DisplayMode[i]);
    if (link == currentTab) {
      link.className = 'pagespeed-underline-link';
    } else {
      link.className = '';
    }
  }

  location.href = location.href.split('#')[0] + '#' + div;
  // TODO(xqyin): Note that there are similar changes under caches page as
  // well. We should factor out the logic and let similar codes like this
  // being shared by different pages.
};


/**
 * Parse the location URL to get its anchor part and show the div element.
 */
pagespeed.Graphs.prototype.parseLocation = function() {
  var div = location.hash.substr(1);
  if (div == '') {
    this.show(pagespeed.Graphs.DisplayDiv.CACHE_APPLIED);
  } else if (goog.object.contains(pagespeed.Graphs.DisplayDiv, div)) {
    this.show(div);
  }
};


/**
 * The option of the display mode of the graphs page. Users can switch modes
 * by the second level navigation bar shown on the page. The page would show
 * the corresponding div elements containing charts.
 * @enum {string}
 */
pagespeed.Graphs.DisplayMode = {
  CACHE_APPLIED: 'cache_applied_mode',
  CACHE_TYPE: 'cache_type_mode',
  IPRO: 'ipro_mode',
  REWRITE_IMAGE: 'image_rewriting_mode',
  REALTIME: 'realtime_mode'
};


/**
 * The id of the div element that should be displayed in each mode. Only the
 * chosen div element would be shown. Others would be hidden.
 * @enum {string}
 */
pagespeed.Graphs.DisplayDiv = {
  CACHE_APPLIED: 'cache_applied',
  CACHE_TYPE: 'cache_type',
  IPRO: 'ipro',
  REWRITE_IMAGE: 'image_rewriting',
  REALTIME: 'realtime'
};


/**
 * Updates the option of auto-refresh.
 */
pagespeed.Graphs.prototype.toggleAutorefresh = function() {
  this.autoRefresh_ = !this.autoRefresh_;
};


/**
 * Generate a URL to request statistics data over default time period.
 * @return {string} The query URL incorporating all time parameters.
 */
pagespeed.Graphs.prototype.createQueryUrl = function() {
  var granularityMs = pagespeed.Graphs.FREQUENCY_ * 1000;
  var durationMs = pagespeed.Graphs.TIMERANGE_ * granularityMs; // 1 Day.
  var endTime = new Date();
  var startTime = new Date(endTime - durationMs);
  var queryString = '?json';
  queryString += '&start_time=' + startTime.getTime();
  queryString += '&end_time=' + endTime.getTime();
  queryString += '&granularity=' + granularityMs;
  return queryString;
};


/**
 * Parses historical data sent by sever.
 * @param {string} text The statistics dumped in JSON format.
 */
pagespeed.Graphs.prototype.parseHistoricalMessages = function(text) {
  var jsonData = JSON.parse(text);
  var timestamps = jsonData['timestamps'];
  var variables = jsonData['variables'];
  for (var i = 0; i < timestamps.length; ++i) {
    var messages = [];
    for (var name in variables) {
      var node = {
        name: name,
        value: variables[name][i]
      };
      messages.push(node);
    }
    var newArray = {
      messages: messages,
      timeReceived: new Date(timestamps[i])
    };
    this.psolMessages_.push(newArray);
  }
};


/**
 * Parses the most recent data sent by sever.
 * @param {string} text The statistics dumped in JSON format.
 */
pagespeed.Graphs.prototype.parseSnapshotMessages = function(text) {
  var jsonData = JSON.parse(text);
  var variables = jsonData['variables'];
  var messages = [];
  for (var name in variables) {
    var node = {
      name: name,
      value: variables[name]
    };
    messages.push(node);
  }
  var newArray = {
    messages: messages,
    timeReceived: new Date()
  };
  this.psolMessages_.push(newArray);
};


/**
 * Capture the pathname and return the URL for request.
 * @return {string} The URL to request the updated data in JSON format.
 */
pagespeed.Graphs.prototype.requestUrl = function() {
  var pathName = location.pathname;
  // Ignore the trailing '/'.
  var n = pathName.lastIndexOf('/', pathName.length - 2);
  // e.g. /pagespeed_admin/foo or pagespeed_admin/foo
  if (n > 0) {
    return pathName.substring(0, n) + '/stats_json';
  } else {
    // e.g. /pagespeed_admin or pagespeed_admin
    return pathName + '/stats_json';
  }
};


/**
 * Refreshes the page by making requsts to server.
 */
pagespeed.Graphs.prototype.performRefresh = function() {
  // TODO(xqyin): Figure out if there is a potential timing issue that the
  // xhr.isActive() would be false before the callback starts.
  if (!this.xhr_.isActive()) {
    // If the first refresh has not started yet, then do the refresh no matter
    // what the autoRefresh option is. Because the page needs to send a query
    // URL to request historical data to get initialized.
    if (!this.firstRefreshStarted_) {
      this.firstRefreshStarted_ = true;
      this.xhr_.send(this.createQueryUrl());
    // The page needs to do the second refresh to finish initialization.
    // Otherwise, only refresh when the autoRefresh option is true.
    } else if (!this.secondRefreshStarted_ || this.autoRefresh_) {
      this.secondRefreshStarted_ = true;
      this.xhr_.send(this.requestUrl());
    }
  }
};


/**
 * Parses the response sent by server and draws charts.
 */
pagespeed.Graphs.prototype.parseAjaxResponse = function() {
  if (this.xhr_.isSuccess()) {
    var text = this.xhr_.getResponseText();
    if (!this.secondRefreshStarted_) {
      // We collect historical data in the first refresh and wait for the
      // second refresh to update data. We won't call functions to draw charts
      // until we make data up-to-date.
      this.parseHistoricalMessages(text);
      // Add the second refresh to the queue with no delay time. So it would be
      // implemented immediately after the first refresh completion.
      window.setTimeout(goog.bind(this.performRefresh, this), 0);
    } else {
      this.parseSnapshotMessages(text);
      // Only keep one-day statistics.
      if (this.psolMessages_.length > pagespeed.Graphs.TIMERANGE_) {
        this.psolMessages_.shift();
      }
      this.drawVisualization();
    }
  } else {
    console.log(this.xhr_.getLastError());
  }
};


/**
 * Initialization for drawing all the charts.
 */
pagespeed.Graphs.prototype.drawVisualization = function() {
  this.drawBarChart('pcache-cohorts-dom', 'Property cache dom cohorts',
                    pagespeed.Graphs.DisplayDiv.CACHE_APPLIED);
  this.drawBarChart('pcache-cohorts-beacon', 'Property cache beacon cohorts',
                    pagespeed.Graphs.DisplayDiv.CACHE_APPLIED);
  this.drawBarChart('rewrite_cached_output', 'Rewrite cached output',
                    pagespeed.Graphs.DisplayDiv.CACHE_APPLIED);
  this.drawBarChart('url_input', 'URL Input',
                    pagespeed.Graphs.DisplayDiv.CACHE_APPLIED);

  this.drawBarChart('cache', 'Cache',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);
  this.drawBarChart('file_cache', 'File Cache',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);
  this.drawBarChart('memcached', 'Memcached',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);
  this.drawBarChart('redis', 'Redis',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);
  this.drawBarChart('lru_cache', 'LRU',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);
  this.drawBarChart('shm_cache', 'Shared Memory',
                    pagespeed.Graphs.DisplayDiv.CACHE_TYPE);

  this.drawBarChart('ipro', 'In place resource optimization',
                    pagespeed.Graphs.DisplayDiv.IPRO);

  this.drawBarChart('image_rewrite', 'Image rewrite',
                    pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE);
  this.drawBarChart('image_rewrites_dropped', 'Image rewrites dropped',
                    pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE);

  this.drawHistoryChart('http', 'Http', pagespeed.Graphs.DisplayDiv.REALTIME);
  this.drawHistoryChart('file_cache', 'File Cache RT',
                        pagespeed.Graphs.DisplayDiv.REALTIME);
  this.drawHistoryChart('lru_cache', 'LRU Cache RT',
                        pagespeed.Graphs.DisplayDiv.REALTIME);
  this.drawHistoryChart('serf_fetch', 'Serf stats RT',
                        pagespeed.Graphs.DisplayDiv.REALTIME);
  this.drawHistoryChart('rewrite', 'Rewrite stats RT',
                        pagespeed.Graphs.DisplayDiv.REALTIME);
};


/**
 * Screens data to generate charts according to the setting prefix.
 * @param {string} prefix The setting prefix to match.
 * @param {string} name The name of the statistics.
 * @return {boolean} Return true if the data should be used in the chart.
 */
pagespeed.Graphs.screenData = function(prefix, name) {
  var use = true;
  if (name.indexOf(prefix) != 0) {
    use = false;
  // We skip here because the statistics below won't be used in any charts.
  } else if (name.indexOf('cache_flush_timestamp_ms') >= 0) {
    use = false;
  } else if (name.indexOf('cache_flush_count') >= 0) {
    use = false;
  } else if (name.indexOf('cache_time_us') >= 0) {
    use = false;
  }
  return use;
};


/**
 * Initialize a chart using Google Charts API.
 * @param {string} id The ID of the chart div to create.
 * @param {string} title The title of the chart.
 * @param {string} chartType The type of chart. AnnotatedTimeLine or BarChart.
 * @param {string} targetId The id of the target HTML element.
 * @return {Object} The created chart object.
 */
pagespeed.Graphs.prototype.initChart = function(
    id, title, chartType, targetId) {
  var theChart;
  // TODO(oschaaf): Title might not be unique
  if (this.chartCache_[title]) {
    theChart = this.chartCache_[title];
  } else {
    // The element identified by the id must exist.
    var targetElement = document.getElementById(targetId);
    // Remove the status info when it starts to draw charts.
    if (targetElement.textContent == 'Loading Charts...') {
      targetElement.textContent = '';
    }
    var dest = document.createElement('div');
    if (chartType == 'AnnotatedTimeLine') {
      dest.className = 'pagespeed-graphs-chart';
    }
    dest.id = id;
    var chartTitle = document.createElement('p');
    chartTitle.textContent = title;

    chartTitle.className = 'pagespeed-graphs-title';
    targetElement.appendChild(chartTitle);
    targetElement.appendChild(dest);
    theChart = new google.visualization[chartType](dest);
    this.chartCache_[title] = theChart;
  }
  return theChart;
};


/**
 * Draw a bar chart using Google Charts API.
 * @param {string} prefix The statistics prefix of data for the chart.
 * @param {string} title The title of the chart.
 * @param {string} targetId The id of the target HTML element.
 */
pagespeed.Graphs.prototype.drawBarChart = function(prefix, title, targetId) {
  var id = 'pagespeed-graphs-' + prefix;
  var settingPrefix = prefix + '_';
  var theChart = this.initChart(id, title, 'BarChart', targetId);
  var dest = document.getElementById(id);
  var rows = [];
  var data = new google.visualization.DataTable();

  var messages = goog.array.clone(
      this.psolMessages_[this.psolMessages_.length - 1].messages);
  var numBars = 0;
  for (var i = 0; i < messages.length; ++i) {
    if (!pagespeed.Graphs.screenData(settingPrefix, messages[i].name)) {
      continue;
    }
    ++numBars;

    // Removes the prefix.
    var caption = messages[i].name.substring(settingPrefix.length);
    // We use regexp here to replace underscores all at once.
    // Using '_' would only replace one underscore at a time.
    caption = caption.replace(/_/g, ' ');
    rows.push([caption, Number(messages[i].value)]);
  }
  data.addColumn('string', 'Name');
  data.addColumn('number', 'Value');
  data.addRows(rows);

  var view = new google.visualization.DataView(data);
  var getStats = function(dataTable, rowNum) {
    var sum = 0;
    for (var i = 0; i < dataTable.getNumberOfRows(); ++i) {
      sum += dataTable.getValue(i, 1);
    }
    var value = dataTable.getValue(rowNum, 1);
    var percent = value * 100 / ((sum == 0) ? 1 : sum);
    return value.toString() + ' (' + percent.toFixed(2).toString() + '%)';
  };
  view.setColumns(
      [0, 1, {'calc': getStats, 'type': 'string', 'role': 'annotation'}]);

  var barHeight = 40 * numBars + 10;
  dest.style.height = barHeight + 20;

  // The options used for drawing google.visualization.barChart graphs.
  var barChartOptions = {
    'annotations': {
      // TODO(jmarantz): Although I kind of prefer the consistency of
      // always showing labels outside the bar, it does waste a bit of
      // horizontal space.  However we have no choice at the moment
      // due to a Charts API bug estimating text sizes in the context
      // of bars that are not high enough to fit two lines of text.
      //
      // Once this bug is fixed (best estimate: Oct 2014) we should
      // consider setting 'alwaysOutside' to 'false', and then we can
      // try changing the chartArea width percentage below from 60% to 80%.
      'alwaysOutside': true,  // Always put the "value(%)" outside the bar.
      'highContrast': true,
      'textStyle': {
        'fontSize': 12,
        'color': 'black'
      }
    },
    'hAxis': {
      'direction': 1
    },
    'vAxis': {
      'textPosition': 'out'   // position of left-most vertical axis label.
    },
    'legend': {
      'position': 'none'
    },
    'width': 800,
    'height': barHeight,
    'chartArea': {
      'left': 225,
      'top': 0,
      'width': '60%',         // Tweak this to avoid clipping outside labels.
      'height': '80%'

    }
  };
  theChart.draw(view, barChartOptions);
};


/**
 * Draw a history chart using Google Charts API.
 * @param {string} prefix The statistics prefix of data for the chart.
 * @param {string} title The title of the chart.
 * @param {string} targetId The id of the target HTML element.
 */
pagespeed.Graphs.prototype.drawHistoryChart = function(
    prefix, title, targetId) {
  var id = 'pagespeed-graphs-' + prefix;
  var settingPrefix = prefix + '_';
  var theChart = this.initChart(id, title, 'AnnotatedTimeLine', targetId);
  var dest = document.getElementById(id);
  var rows = [];
  var data = new google.visualization.DataTable();

  // The annotated time line for data history.
  data.addColumn('datetime', 'Time');
  var first = true;
  for (var i = 0; i < this.psolMessages_.length; ++i) {
    var messages = goog.array.clone(this.psolMessages_[i].messages);
    var row = [];
    row.push(this.psolMessages_[i].timeReceived);
    for (var j = 0; j < messages.length; ++j) {
      if (!pagespeed.Graphs.screenData(settingPrefix, messages[j].name)) {
        continue;
      }
      row.push(Number(messages[j].value));
      if (first) {
        var caption = messages[j].name.substring(settingPrefix.length);
        caption = caption.replace(/_/g, ' ');
        data.addColumn('number', caption);
      }
    }
    first = false;
    rows.push(row);
  }
  data.addRows(rows);
  theChart.draw(data, pagespeed.Graphs.ANNOTATED_TIMELINE_OPTIONS_);
};


/**
 * The options used for drawing google.visualization.AnnotatedTimeLine graphs.
 * @private {Object}
 * @const
 */
pagespeed.Graphs.ANNOTATED_TIMELINE_OPTIONS_ = {
  'thickness': 1,
  'displayExactValues': true,
  'legendPosition': 'newRow'
};


/**
 * The frequency of auto-refresh. Default is once per 5 seconds.
 * @private {number}
 * @const
 */
pagespeed.Graphs.FREQUENCY_ = 5;


/**
 * The size limit to the data array. Default is one day.
 * @private {number}
 * @const
 */
pagespeed.Graphs.TIMERANGE_ = 24 * 60 * 60 /
                                  pagespeed.Graphs.FREQUENCY_;


/**
 * The Main entry to start processing.
 * @export
 */
pagespeed.Graphs.Start = function() {
  var graphsOnload = function() {
    var graphsObj = new pagespeed.Graphs();
    graphsObj.parseLocation();
    for (var i in pagespeed.Graphs.DisplayDiv) {
      goog.events.listen(
          document.getElementById(pagespeed.Graphs.DisplayMode[i]), 'click',
          goog.bind(graphsObj.show, graphsObj, pagespeed.Graphs.DisplayDiv[i]));
    }
    // IE6/7 don't support this event. Then the back button would not work
    // because no requests captured.
    goog.events.listen(window, 'hashchange',
                       goog.bind(graphsObj.parseLocation, graphsObj));

    goog.events.listen(document.getElementById('auto-refresh'), 'change',
                       goog.bind(graphsObj.toggleAutorefresh,
                                 graphsObj));
    // We call listen() here so this listener is added to the xhr only once.
    // If we call listen() inside performRefresh() method, we are adding a new
    // listener to the xhr every time it auto-refreshes, which would cause
    // fetchContent() being called multiple times. Users will see an obvious
    // delay because we draw the same charts multiple times in one refresh.
    goog.events.listen(
        graphsObj.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(graphsObj.parseAjaxResponse, graphsObj));
    setInterval(goog.bind(graphsObj.performRefresh, graphsObj),
                pagespeed.Graphs.FREQUENCY_ * 1000);
    graphsObj.performRefresh();
  };
  goog.events.listen(window, 'load', graphsOnload);
};
