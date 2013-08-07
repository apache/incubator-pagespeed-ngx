// Copyright 2011 Google Inc. All Rights Reserved.

/**
 * @fileoverview Client side logic for loading panels.
 * TODO(gagansingh): Add TTI, CSI, benchmark, Tests.
 * @author gagansingh@google.com (Gagan Singh)
 */

// States.
var CRITICAL_DATA_LOADED = 'cdl';
var NON_CRITICAL_LOADED = 'ncl';

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * Class for loading panels.
 *
 * @constructor
 */
pagespeed.PanelLoader = function() {
  this.readyToLoadNonCritical = false;

  this.delayedNonCriticalData = null;
  this.nonCriticalData = {};
  this.nonCriticalDataPresent = false;
  this.nonCacheablePanelInstances = {};
  this.pageManager = new PageManager();

  this.dashboardDisplayTime = 0;
  this.csiTimings = {time: {}, size: {}};

  this.contentSizeKb = 0;
  this.debugIp = false;
  // Using navigation timing api for start time. This api is available in
  // Internet Explorer 9, Google Chrome 6 and Firefox 7.
  // If not present, we trigger the start time when the rendering started.
  if (window.performance) {
    // timeStart will be number of milliseconds
    this.timeStart = window.performance.timing.navigationStart;
  } else {
    this.timeStart = window.mod_pagespeed_start;
  }

  this.changePageLoadState(CRITICAL_DATA_LOADED, 0);
};

/**
 * Main state machine for loading panels.
 * NOTE: This function assumes the following order of calls:
 * Critical data --> Critical images --> Callback for critical low res
 *   --> Callback for critical hi res --> Non critical data
 */
pagespeed.PanelLoader.prototype.loadData = function() {
  if (this.nonCriticalDataPresent && this.readyToLoadNonCritical &&
      this.state != NON_CRITICAL_LOADED) {
      this.pageManager.instantiatePage(this.nonCriticalData);
    // Remove 'DONT_BIND' in all non-cacheable objects.
    for (var panelId in this.nonCacheablePanelInstances) {
      if (!this.nonCacheablePanelInstances.hasOwnProperty(panelId)) continue;
      var panelData = this.nonCacheablePanelInstances[panelId];
      for (var i = 0; i < panelData.length; ++i) {
        panelData[i][DONT_BIND] = false;
      }
    }
    // Load Non-Critical Non-Cacheable Panels.
    this.pageManager.instantiatePage(this.nonCacheablePanelInstances);

    this.changePageLoadState(NON_CRITICAL_LOADED);

    // Scroll to the hash fragment when it is given, as it can be found
    // in Non-Critical panels loaded just now.
    var hash = window.location.hash;
    if (hash && hash[0] == '#') {
      // Make sure the hash fragment refers to an element, since it can
      // be "parameters" of Ajax applications.
      if (document.getElementById(hash.slice(1))) {
        // Use window.location.replace() here rather than a more popular
        // technique <element>.scrollIntoView() to let browsers track on
        // the element even if it gets moved/resized later on by scripts,
        // such as deferJs and window.setTimeout().
        window.location.replace(hash);
      }
    }

    if (window.pagespeed && window.pagespeed.deferJs) {
      window.pagespeed.deferJs.registerScriptTags();
      var scriptExecutor = function() {
        window.pagespeed.deferJs.run();
      };
      // NOTE: setTimeout can cause DOMContentLoaded to be fired before scripts
      // start executing, causing problems with JQuery. Not much can be done
      // because delaying <script src> have similar side effects, hence just
      // making a note of the issue.
      setTimeout(scriptExecutor, 1);
    }
    return;
  }
};

pagespeed.PanelLoader.prototype['loadData'] =
    pagespeed.PanelLoader.prototype.loadData;


/**
 * Determines whether the given state means that client is still rendering
 * critical portions.
 * @param {string} state
 * @return {boolean}
 */
pagespeed.PanelLoader.prototype.isStateInCriticalPath = function(state) {
  return state == CRITICAL_DATA_LOADED;
};

/**
 * Accessor for csi timings as a string.
 * @return {string} Returns the csi timing as a string.
 */
pagespeed.PanelLoader.prototype.getCsiTimingsString = function() {
  var csiTimingStr = '';
  for (var state in this.csiTimings.time) {
    csiTimingStr += state + '.' + this.csiTimings.time[state] + ',';
  }
  for (var state in this.csiTimings.size) {
    csiTimingStr += state + '_sz.' + this.csiTimings.size[state] + ',';
  }
  return csiTimingStr;
};
pagespeed.PanelLoader.prototype['getCsiTimingsString'] =
    pagespeed.PanelLoader.prototype.getCsiTimingsString;

/**
 * Updates the dashboard.
 */
pagespeed.PanelLoader.prototype.updateDashboard = function() {
  var timeNow = new Date();
  var dateElem = document.getElementById('dashboard_area') ||
      window['dashboard_area'];
  if (this.debugIp ||
      (window.localStorage && window.localStorage['psa_debug'] == '1')) {
    if (!dateElem) {
      dateElem = document.createElement('div');
      dateElem.id = 'dashboard_area';
      dateElem.style.color = 'gray';
      dateElem.style.fontSize = '10px';
      dateElem.style.fontFace = 'Arial';
      dateElem.style.backgroundColor = 'white';
      document.body.insertBefore(dateElem, document.body.childNodes[0]);
    }
    var timings = 'TIME:\n' + JSON.stringify(this.csiTimings.time)
        .replace(/["{}]/g, '').replace(/,/g, ' ');
    timings += '\nSIZE:\n' + JSON.stringify(this.csiTimings.size)
        .replace(/["{}]/g, '').replace(/,/g, ' ');
    dateElem.innerHTML =
        '<span title="' + timings + '">' + this.dashboardDisplayTime + 'ms; ' +
        this.contentSizeKb.toFixed() + 'KB;' + timeNow.toGMTString() +
        '</span>';
  }
};

/**
 * Updates the state and the dashboard with the debugging info.
 * @param {string} newState
 * @param {number} opt_size
 */
pagespeed.PanelLoader.prototype.changePageLoadState = function(newState,
                                                               opt_size) {
  this.state = newState;
  var timeNow = new Date();
  var timeTaken = (timeNow - this.timeStart);
  this.addCsiTiming(newState, timeTaken, opt_size);

  if (this.isStateInCriticalPath(newState)) {
    this.contentSizeKb += opt_size ? (opt_size / 1024) : 0;
    this.dashboardDisplayTime = timeTaken;
  }
  this.updateDashboard();
};

/**
 * Executes above the fold scripts till a panel stub is encountered.
 */
pagespeed.PanelLoader.prototype.executeATFScripts = function() {
  if (window.pagespeed && window.pagespeed.deferJs) {
    var me = this;
    var criticalScriptsDoneCallback = function() {
      me.criticalScriptsDone();
    };
    window.pagespeed.deferJs.registerScriptTags(
        criticalScriptsDoneCallback, pagespeed.lastScriptIndexBeforePanelStub);
    // TODO(ksimbili): Wait untill the high res Images are set before starting
    // the execution.
    window.pagespeed.deferJs.run();
  }
};

// -------------------------- API EXPOSED --------------------------------
/**
 * Sets the request from internal ip.
 */
pagespeed.PanelLoader.prototype.setRequestFromInternalIp = function() {
  this.debugIp = true;
};
pagespeed.PanelLoader.prototype['setRequestFromInternalIp'] =
    pagespeed.PanelLoader.prototype.setRequestFromInternalIp;

/**
 * Adds debugging info like timing and size to the dashboard.
 * TODO(ksimbili,anupama): Start adding the timing information into pagespeed
 * global variables instead of local ones.
 * @param {string} variable
 * @param {number} time
 * @param {number} opt_size
 */
pagespeed.PanelLoader.prototype.addCsiTiming = function(variable, time,
                                                        opt_size) {
  this.csiTimings.time[variable] = time;
  if (opt_size) this.csiTimings.size[variable] = opt_size;
};

/**
 * Loads cookies.
 * @param {Array.<string>} data
 */
pagespeed.PanelLoader.prototype.loadCookies = function(data) {
  for (var i = 0; i < data.length; i++) {
    document.cookie = data[i];
  }
};
pagespeed.PanelLoader.prototype['loadCookies'] =
    pagespeed.PanelLoader.prototype.loadCookies;

/**
 * Attempts to load non-cacheable object. If failed will buffer the object and
 * try again after Non-critical is loaded.
 * @param {Object} data Non-Critical Data.
 */
pagespeed.PanelLoader.prototype.loadNonCacheableObject = function(data) {
  if (this.state == NON_CRITICAL_LOADED) {
    return;
  }
  for (var panelId in data) {
    if (!data.hasOwnProperty(panelId)) continue;
    this.nonCacheablePanelInstances[panelId] =
        this.nonCacheablePanelInstances[panelId] || [];
    this.nonCacheablePanelInstances[panelId].push(data[panelId]);
    var endPanelStubs = getPanelStubs(getDocument().documentElement,
                                      getDocument().documentElement,
                                      panelId);

    if (endPanelStubs.length > 0) {
      this.pageManager.instantiatePage(this.nonCacheablePanelInstances);

      this.nonCacheablePanelInstances[panelId].pop();
      var newInstance = {};
      this.nonCacheablePanelInstances[panelId].push(newInstance);
    } else {
      data[panelId][DONT_BIND] = true;
    }
  }
};
pagespeed.PanelLoader.prototype['loadNonCacheableObject'] =
    pagespeed.PanelLoader.prototype.loadNonCacheableObject;

/**
 * Callback function for DeferJs, when critical scripts are done.
 */
pagespeed.PanelLoader.prototype.criticalScriptsDone = function() {
  this.readyToLoadNonCritical = true;
  this.loadData();
};
pagespeed.PanelLoader.prototype['criticalScriptsDone'] =
    pagespeed.PanelLoader.prototype.criticalScriptsDone;

/**
 * Buffers the non critical data and loads it if hi res images are loaded.
 * @param {Object} data Non-Critical Data.
 * @param {boolean} opt_delay_bind if set to true, will save the data passed in
 *    and use the saved data that when the function is called again without
 *    opt_delay_bind (note: will overwrite whatever data is passed in at that
 *    point with the saved value if something was saved).
 */
pagespeed.PanelLoader.prototype.bufferNonCriticalData = function(
    data, opt_delay_bind) {
  if (opt_delay_bind) {
    this.delayedNonCriticalData = data;
    return;
  } else if (this.delayedNonCriticalData) {
    data = this.delayedNonCriticalData;
  }
  if (this.state == NON_CRITICAL_LOADED) {
    return;
  }
  this.nonCriticalData = data;
  this.nonCriticalDataPresent = true;
  this.loadData();
};
pagespeed.PanelLoader.prototype['bufferNonCriticalData'] =
    pagespeed.PanelLoader.prototype.bufferNonCriticalData;

/**
 * Iniitialize the panel loader.
 */
pagespeed.panelLoaderInit = function() {
  if (pagespeed['panelLoader']) {
    return;
  }
  var ctx = new pagespeed.PanelLoader();
  pagespeed['panelLoader'] = ctx;
  ctx.executeATFScripts();
};
pagespeed['panelLoaderInit'] = pagespeed.panelLoaderInit;
