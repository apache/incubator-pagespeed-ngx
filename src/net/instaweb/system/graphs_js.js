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
 * @fileoverview Code for adding auto-refreshing graphs and realtime linecharts
 * on the basis of the data in statistics page.
 *
 * TODO(xqyin): Integrate the raw mode to statistics page and other modes to
 * console page.
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
google.load('visualization', '1', {'packages': ['table', 'corechart']});


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
   * This array of arrays collects data from statistics page since last
   * refresh. The i-th entry of the array is the statistics fetched in
   * the i-th refresh, which is an array of special nodes. Each node contains
   * certain statistics name and the value. We use the last entry to
   * generate pie charts showing the most recent data. The whole array of
   * arrays is used to generate line charts showing statistics history.
   * We only keep one-day data to avoid infinite growth of this array.
   * @private {!Array.<pagespeed.StatsArray>}
   */
  this.psolMessages_ = [];
  // TODO(xqyin): Using ConsoleJsonHandler to provide data so we won't lose
  // data after refreshing the page manually.

  /**
   * We provide filtering functionality for users to search for certain
   * statistics like all the statistics related to caches. This option is the
   * matching pattern. Filtering is case-insensitive.
   * @private {string}
   */
  this.filter_ = '';

  /**
   * The option of auto-refresh. If true, the page will automatically refresh
   * itself. The default frequency is to refresh every 5 seconds.
   * @private {boolean}
   */
  this.autoRefresh_ = true;

  /**
   * The flag of whether the first refresh is done. We need to call a refresh
   * when loading the page to finish the initialization. Default is false,
   * which means the page hasn't done the first refresh yet.
   * @private {boolean}
   */
  this.firstRefreshDone_ = false;

  // The navigation bar to switch among different display modes
  // TODO(xqyin): Consider making these different tabs query params.
  var navElement = document.createElement('table');
  navElement.id = 'navBar';
  navElement.innerHTML =
      '<tr><td><a id="' + pagespeed.Graphs.DisplayMode.RAW +
      '" href="javascript:void(0);">Raw</a> - </td>' +
      '<td><a id="' + pagespeed.Graphs.DisplayMode.CACHE_APPLIED +
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
  // The UI table of auto-refresh and filtering.
  var uiTable = document.createElement('div');
  uiTable.id = 'uiDiv';
  uiTable.innerHTML =
      '<table id="uiTable" border=1 style="border-collapse: ' +
      'collapse;border-color:silver;"><tr valign="center">' +
      '<td>Auto refresh: <input type="checkbox" id="autoRefresh" ' +
      (this.autoRefresh_ ? 'checked' : '') + '></td>' +
      '<td>&nbsp;&nbsp;&nbsp;&nbsp;Search: ' +
      '<input id="txtFilter" type="text"></td>' +
      '</tr></table>';
  document.body.insertBefore(
      uiTable, document.getElementById(pagespeed.Graphs.DisplayDiv.RAW));
  document.body.insertBefore(navElement, document.getElementById('uiDiv'));
};


/**
 * Show the chosen div element and hide all other elements.
 * @param {string} div The chosen div element to display.
 */
pagespeed.Graphs.prototype.show = function(div) {
  // Hides all the elements first.
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.RAW).style.display = 'none';
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.CACHE_APPLIED).style.display = 'none';
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.CACHE_TYPE).style.display = 'none';
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.IPRO).style.display = 'none';
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE).style.display = 'none';
  document.getElementById(
      pagespeed.Graphs.DisplayDiv.REALTIME).style.display = 'none';
  // Only shows the element chosen by users.
  document.getElementById(div).style.display = '';
  // If the chosen display mode is 'raw', then display the UI table for
  // auto-refresh and statistics filtering.
  document.getElementById('uiTable').style.display =
      (div == pagespeed.Graphs.DisplayDiv.RAW) ? '' : 'none';
};


/**
 * The option of the display mode of the graphs page. Users can switch modes
 * by the second level navigation bar shown on the page. In the default raw
 * mode it would show the raw div element which is the statistics in text.
 * When it is set to other values, it would show the corresponding div elements
 * containing charts.
 * @enum {string}
 */
pagespeed.Graphs.DisplayMode = {
  RAW: 'raw_mode',
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
  RAW: 'raw',
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
 * Updates the option of statistics filter.
 * @param {!HTMLElement} element The HTML element of Filter.
 */
pagespeed.Graphs.prototype.setFilter = function(element) {
  this.filter_ = element.value;
  this.update();
};


/**
 * Filters and displays raw statistics according to the options.
 */
pagespeed.Graphs.prototype.update = function() {
  // The most recent data.
  var messages = goog.array.clone(
      this.psolMessages_[this.psolMessages_.length - 1].messages);

  if (this.filter_) {
    for (var i = messages.length - 1; i >= 0; --i) {
      if (!messages[i].name ||
          !goog.string.caseInsensitiveContains(
              messages[i].name, this.filter_)) {
        messages.splice(i, 1);
      }
    }
  }

  var rawElement = document.getElementById(pagespeed.Graphs.DisplayDiv.RAW);
  rawElement.innerHTML = '';
  var table = document.createElement('table');
  table.style.display = 'text-align: left;';
  rawElement.appendChild(table);
  for (var i = 0; i < messages.length; ++i) {
    var tr = document.createElement('tr');
    table.appendChild(tr);
    var tdName = document.createElement('td');
    var tdValue = document.createElement('td');
    tr.appendChild(tdName);
    tr.appendChild(tdValue);
    tdName.innerText = messages[i].name + ':';
    tdValue.innerText = messages[i].value;
  }
  this.drawVisualization();
};


/**
  * The error message of dump failure.
  * @private {pagespeed.StatsNode}
  * @const
  */
pagespeed.Graphs.DUMP_ERROR_ = {
  name: 'Error',
  value: 'Failed to write statistics to this page.'
};


/**
 * Parses new statistics from the server response.
 * @param {string} text The raw text content sent by server.
 * The expected format of text should be like this:
 * <html>
 *   <head>...</head>
 *   <body>
 *     <div style=...>...</div><hr>
 *     <pre>...</pre>
 *   </body>
 * </html>
 * @return {!pagespeed.StatsArray} messages The updated statistics list.
 */
pagespeed.Graphs.prototype.parseMessagesFromResponse = function(text) {
  // TODO(xqyin): Use ConsoleJsonHandler to provide pure statistics instead
  // of parsing the HTML here.
  var messages = [];
  var timeReceived = null;
  var rawString = [];
  var start = text.indexOf('<pre>');
  var end = text.indexOf('</pre>', start);
  if (start >= 0 && end >= 0) {
    start = start + '<pre>'.length;
    end = end - 1;
    rawString = text.substring(start, end).split('\n');
    for (var i = 0; i < rawString.length; ++i) {
      var tmp = rawString[i].split(':');
      if (!tmp[0] || !tmp[1]) continue;
      var node = {
        name: tmp[0].trim(),
        value: tmp[1].trim()
      };
      messages[messages.length] = node;
    }
    timeReceived = new Date();
  } else {
    messages.push(pagespeed.Graphs.DUMP_ERROR_);
  }
  var newArray = {
    messages: messages,
    timeReceived: timeReceived
  };
  return newArray;
};


/**
  * The error message of auto-refresh.
  * @private {string}
  * @const
  */
pagespeed.Graphs.REFRESH_ERROR_ =
    'Sorry, failed to update the statistics. ' +
    'Please wait and try again later.';


/**
 * Refreshs the page by making requsts to server.
 */
pagespeed.Graphs.prototype.performRefresh = function() {
  // If the first refresh has not done yet, then do the refresh no matter what
  // the autoRefresh option is. Because the page needs at least one refresh to
  // finish initializatoin. Otherwise, check the autoRefresh option and the
  // current refreshing status.
  if (!this.xhr_.isActive() &&
      (!this.firstRefreshDone_ || this.autoRefresh_)) {
    var fetchContent = function(graphsObj) {
      if (this.isSuccess()) {
        var newText = this.getResponseText();
        graphsObj.psolMessages_.push(
            graphsObj.parseMessagesFromResponse(newText));
        // Only keep one-day statistics.
        if (graphsObj.psolMessages_.length >
            pagespeed.Graphs.TIMERANGE_) {
          graphsObj.psolMessages_.shift();
        }
        graphsObj.update();
      } else {
        console.log(this.getLastError());
        document.getElementById(pagespeed.Graphs.DisplayDiv.RAW).innerText =
            pagespeed.Graphs.REFRESH_ERROR_;
      }
      graphsObj.firstRefreshDone_ = true;
    };
    goog.events.listen(
        this.xhr_, goog.net.EventType.COMPLETE,
        goog.bind(fetchContent, this.xhr_, this));
    this.xhr_.send('/pagespeed_admin/statistics');
  }
};


/**
 * Initialization for drawing all the charts.
 */
pagespeed.Graphs.prototype.drawVisualization = function() {
  var prefixes = [
    ['pcache-cohorts-dom_', 'Property cache dom cohorts', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_APPLIED],
    ['pcache-cohorts-beacon_', 'Property cache beacon cohorts', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_APPLIED],
    ['rewrite_cached_output_', 'Rewrite cached output', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_APPLIED],
    ['rewrite_', 'Rewrite', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_APPLIED],
    ['url_input_', 'URL Input', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_APPLIED],

    ['cache_', 'Cache', 'PieChart', pagespeed.Graphs.DisplayDiv.CACHE_TYPE],
    ['file_cache_', 'File Cache', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_TYPE],
    ['memcached_', 'Memcached', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_TYPE],
    ['lru_cache_', 'LRU', 'PieChart', pagespeed.Graphs.DisplayDiv.CACHE_TYPE],
    ['shm_cache_', 'Shared Memory', 'PieChart',
     pagespeed.Graphs.DisplayDiv.CACHE_TYPE],

    ['ipro_', 'In place resource optimization', 'PieChart',
     pagespeed.Graphs.DisplayDiv.IPRO],

    ['image_rewrite_', 'Image rewrite', 'PieChart',
     pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE],
    ['image_rewrites_dropped_', 'Image rewrites dropped', 'PieChart',
     pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE],

    ['http_', 'Http', 'LineChart', pagespeed.Graphs.DisplayDiv.REALTIME, true],
    ['file_cache_', 'File Cache RT', 'LineChart',
     pagespeed.Graphs.DisplayDiv.REALTIME, true],
    ['lru_cache_', 'LRU Cache RT', 'LineChart',
     pagespeed.Graphs.DisplayDiv.REALTIME, true],
    ['serf_fetch_', 'Serf stats RT', 'LineChart',
     pagespeed.Graphs.DisplayDiv.REALTIME, true],
    ['rewrite_', 'Rewrite stats RT', 'LineChart',
     pagespeed.Graphs.DisplayDiv.REALTIME, true]
  ];

  for (var i = 0; i < prefixes.length; ++i) {
    this.drawChart(prefixes[i][0], prefixes[i][1], prefixes[i][2],
                   prefixes[i][3], prefixes[i][4]);
  }
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
 * Draw the chart using Google Charts API.
 * @param {string} settingPrefix The matching prefix of data for the chart.
 * @param {string} title The title of the chart.
 * @param {string} chartType The type of the chart. LineChart or PieChart.
 * @param {string} targetId The id of the target HTML element.
 * @param {boolean} showHistory The flag of history line charts.
 */
pagespeed.Graphs.prototype.drawChart = function(settingPrefix, title,
                                                chartType, targetId,
                                                showHistory) {
  this.drawChart.chartCache = this.drawChart.chartCache ?
                              this.drawChart.chartCache : {};
  var theChart;
  // TODO(oschaaf): Title might not be unique
  if (this.drawChart.chartCache[title]) {
    theChart = this.drawChart.chartCache[title];
  } else {
    // The element identified by the id must exist.
    var targetElement = document.getElementById(targetId);
    var dest = document.createElement('div');
    dest.className = 'chart';
    targetElement.appendChild(dest);
    theChart = new google.visualization[chartType](dest);
    this.drawChart.chartCache[title] = theChart;
  }

  var rows = [];
  var data = new google.visualization.DataTable();

  // The graphs for recentest data.
  if (!showHistory) {
    var messages = goog.array.clone(
        this.psolMessages_[this.psolMessages_.length - 1].messages);
    for (var i = 0; i < messages.length; ++i) {
      if (messages[i].value == '0') continue;
      if (!pagespeed.Graphs.screenData(settingPrefix, messages[i].name)) {
        continue;
      }
      // Removes the prefix.
      var caption = messages[i].name.substring(settingPrefix.length);
      // We use regexp here to replace underscores all at once.
      // Using '_' would only replace one underscore at a time.
      caption = caption.replace(/_/ig, ' ');
      rows.push([caption, Number(messages[i].value)]);
    }
    data.addColumn('string', 'Name');
    data.addColumn('number', 'Value');
  } else {
    // The line charts for data history.
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
          caption = caption.replace(/_/ig, ' ');
          data.addColumn('number', caption);
        }
      }
      first = false;
      rows.push(row);
    }
  }

  // TODO(oschaaf): Merge this with options from an argument or some such.
  var options = {
    'width': 1000,
    'height': 300,
    'chartArea': {
      'left': 100,
      'top': 50,
      'width': 700
    },
    title: title
  };
  data.addRows(rows);
  theChart.draw(data, options);
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
    var filterElement = document.getElementById('txtFilter');
    goog.events.listen(
        filterElement, 'keyup', goog.bind(graphsObj.setFilter,
                                          graphsObj, filterElement));
    goog.events.listen(document.getElementById('autoRefresh'), 'change',
                       goog.bind(graphsObj.toggleAutorefresh,
                                 graphsObj));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.RAW), 'click',
        goog.bind(graphsObj.show, graphsObj, pagespeed.Graphs.DisplayDiv.RAW));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.CACHE_APPLIED),
        'click',
        goog.bind(graphsObj.show, graphsObj,
                  pagespeed.Graphs.DisplayDiv.CACHE_APPLIED));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.CACHE_TYPE),
        'click',
        goog.bind(graphsObj.show, graphsObj,
                  pagespeed.Graphs.DisplayDiv.CACHE_TYPE));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.IPRO), 'click',
        goog.bind(graphsObj.show, graphsObj,
                  pagespeed.Graphs.DisplayDiv.IPRO));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.REWRITE_IMAGE),
        'click',
        goog.bind(graphsObj.show, graphsObj,
                  pagespeed.Graphs.DisplayDiv.REWRITE_IMAGE));
    goog.events.listen(
        document.getElementById(pagespeed.Graphs.DisplayMode.REALTIME),
        'click',
        goog.bind(graphsObj.show, graphsObj,
                  pagespeed.Graphs.DisplayDiv.REALTIME));
    setInterval(graphsObj.performRefresh.bind(graphsObj),
                pagespeed.Graphs.FREQUENCY_ * 1000);
    graphsObj.performRefresh();
  };
  goog.events.listen(window, 'load', graphsOnload);
};
