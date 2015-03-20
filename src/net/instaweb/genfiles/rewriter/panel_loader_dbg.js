(function(){var CRITICAL_DATA_LOADED = "cdl", NON_CRITICAL_LOADED = "ncl";
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.PanelLoader = function() {
  this.readyToLoadNonCritical = !1;
  this.delayedNonCriticalData = null;
  this.nonCriticalData = {};
  this.nonCriticalDataPresent = !1;
  this.nonCacheablePanelInstances = {};
  this.pageManager = new PageManager;
  this.dashboardDisplayTime = 0;
  this.csiTimings = {time:{}, size:{}};
  this.contentSizeKb = 0;
  this.debugIp = !1;
  this.timeStart = window.performance ? window.performance.timing.navigationStart : window.mod_pagespeed_start;
  this.changePageLoadState(CRITICAL_DATA_LOADED, 0);
};
pagespeed.PanelLoader.prototype.loadData = function() {
  if (this.nonCriticalDataPresent && this.readyToLoadNonCritical && this.state != NON_CRITICAL_LOADED) {
    this.pageManager.instantiatePage(this.nonCriticalData);
    for (var a in this.nonCacheablePanelInstances) {
      if (this.nonCacheablePanelInstances.hasOwnProperty(a)) {
        for (var b = this.nonCacheablePanelInstances[a], c = 0;c < b.length;++c) {
          b[c][DONT_BIND] = !1;
        }
      }
    }
    this.pageManager.instantiatePage(this.nonCacheablePanelInstances);
    this.changePageLoadState(NON_CRITICAL_LOADED);
    (a = window.location.hash) && "#" == a[0] && document.getElementById(a.slice(1)) && window.location.replace(a);
    window.pagespeed && window.pagespeed.deferJs && (window.pagespeed.deferJs.registerScriptTags(), setTimeout(function() {
      window.pagespeed.deferJs.run();
    }, 1));
  }
};
pagespeed.PanelLoader.prototype.loadData = pagespeed.PanelLoader.prototype.loadData;
pagespeed.PanelLoader.prototype.isStateInCriticalPath = function(a) {
  return a == CRITICAL_DATA_LOADED;
};
pagespeed.PanelLoader.prototype.getCsiTimingsString = function() {
  var a = "", b;
  for (b in this.csiTimings.time) {
    a += b + "." + this.csiTimings.time[b] + ",";
  }
  for (b in this.csiTimings.size) {
    a += b + "_sz." + this.csiTimings.size[b] + ",";
  }
  return a;
};
pagespeed.PanelLoader.prototype.getCsiTimingsString = pagespeed.PanelLoader.prototype.getCsiTimingsString;
pagespeed.PanelLoader.prototype.updateDashboard = function() {
  var a = new Date, b = document.getElementById("dashboard_area") || window.dashboard_area;
  if (this.debugIp || window.localStorage && "1" == window.localStorage.psa_debug) {
    b || (b = document.createElement("div"), b.id = "dashboard_area", b.style.color = "gray", b.style.fontSize = "10px", b.style.fontFace = "Arial", b.style.backgroundColor = "white", document.body.insertBefore(b, document.body.childNodes[0]));
    var c = "TIME:\n" + JSON.stringify(this.csiTimings.time).replace(/["{}]/g, "").replace(/,/g, " "), c = c + ("\nSIZE:\n" + JSON.stringify(this.csiTimings.size).replace(/["{}]/g, "").replace(/,/g, " "));
    b.innerHTML = '<span title="' + c + '">' + this.dashboardDisplayTime + "ms; " + this.contentSizeKb.toFixed() + "KB;" + a.toGMTString() + "</span>";
  }
};
pagespeed.PanelLoader.prototype.changePageLoadState = function(a, b) {
  this.state = a;
  var c = new Date - this.timeStart;
  this.addCsiTiming(a, c, b);
  this.isStateInCriticalPath(a) && (this.contentSizeKb += b ? b / 1024 : 0, this.dashboardDisplayTime = c);
  this.updateDashboard();
};
pagespeed.PanelLoader.prototype.executeATFScripts = function() {
  if (window.pagespeed && window.pagespeed.deferJs) {
    var a = this;
    window.pagespeed.deferJs.registerScriptTags(function() {
      a.criticalScriptsDone();
    }, pagespeed.lastScriptIndexBeforePanelStub);
    window.pagespeed.deferJs.run();
  }
};
pagespeed.PanelLoader.prototype.setRequestFromInternalIp = function() {
  this.debugIp = !0;
};
pagespeed.PanelLoader.prototype.setRequestFromInternalIp = pagespeed.PanelLoader.prototype.setRequestFromInternalIp;
pagespeed.PanelLoader.prototype.addCsiTiming = function(a, b, c) {
  this.csiTimings.time[a] = b;
  c && (this.csiTimings.size[a] = c);
};
pagespeed.PanelLoader.prototype.loadCookies = function(a) {
  for (var b = 0;b < a.length;b++) {
    document.cookie = a[b];
  }
};
pagespeed.PanelLoader.prototype.loadCookies = pagespeed.PanelLoader.prototype.loadCookies;
pagespeed.PanelLoader.prototype.loadNonCacheableObject = function(a) {
  if (this.state != NON_CRITICAL_LOADED) {
    for (var b in a) {
      a.hasOwnProperty(b) && (this.nonCacheablePanelInstances[b] = this.nonCacheablePanelInstances[b] || [], this.nonCacheablePanelInstances[b].push(a[b]), 0 < getPanelStubs(getDocument().documentElement, getDocument().documentElement, b).length ? (this.pageManager.instantiatePage(this.nonCacheablePanelInstances), this.nonCacheablePanelInstances[b].pop(), this.nonCacheablePanelInstances[b].push({})) : a[b][DONT_BIND] = !0);
    }
  }
};
pagespeed.PanelLoader.prototype.loadNonCacheableObject = pagespeed.PanelLoader.prototype.loadNonCacheableObject;
pagespeed.PanelLoader.prototype.criticalScriptsDone = function() {
  this.readyToLoadNonCritical = !0;
  this.loadData();
};
pagespeed.PanelLoader.prototype.criticalScriptsDone = pagespeed.PanelLoader.prototype.criticalScriptsDone;
pagespeed.PanelLoader.prototype.bufferNonCriticalData = function(a, b) {
  b ? this.delayedNonCriticalData = a : (this.delayedNonCriticalData && (a = this.delayedNonCriticalData), this.state != NON_CRITICAL_LOADED && (this.nonCriticalData = a, this.nonCriticalDataPresent = !0, this.loadData()));
};
pagespeed.PanelLoader.prototype.bufferNonCriticalData = pagespeed.PanelLoader.prototype.bufferNonCriticalData;
pagespeed.panelLoaderInit = function() {
  if (!pagespeed.panelLoader) {
    var a = new pagespeed.PanelLoader;
    pagespeed.panelLoader = a;
    a.executeATFScripts();
  }
};
pagespeed.panelLoaderInit = pagespeed.panelLoaderInit;
var PANEL_ID = "panel-id", PANEL_STUBSTART = "GooglePanel begin ", PANEL_STUBEND = "GooglePanel end ", INSTANCE_HTML = "instance_html", CONTIGUOUS = "contiguous", XPATH = "xpath", DONT_BIND = "dont_bind", IMAGES = "images", BLINK_SRC = "pagespeed_high_res_src", PANEL_MARKER = "psa_disabled";
function CHECK(a) {
  if (!a) {
    throw "CHECK failed";
  }
}
var getDocument = function() {
  return document;
};
function isInternetExplorer() {
  return "Microsoft Internet Explorer" == navigator.appName;
}
function createInnerHtmlElements(a, b) {
  "HEAD" == a && (a = "div");
  var c;
  !isInternetExplorer() || "TABLE" != a && "TBODY" != a ? (c = getDocument().createElement(a), c.innerHTML = b) : (c = getDocument().createElement("div"), c.innerHTML = "<table>" + b + "</table>", c = c.getElementsByTagName("tbody")[0]);
  for (var d = getDocument().createDocumentFragment();0 < c.childNodes.length;) {
    c.childNodes[0].tagName && (c.setAttribute("psa_not_processed", ""), c.setAttribute("priority_psa_not_processed", "")), d.appendChild(c.childNodes[0]);
  }
  return d;
}
function getMatchingXPathInDom(a, b) {
  for (var c = getDocument().evaluate(b, a, null, XPathResult.ORDERED_NODE_ITERATOR_TYPE, null), d = [], e;e = c.iterateNext();d.push(e)) {
  }
  return d;
}
function getElementsByTagAndAttribute(a, b) {
  for (var c = getDocument().documentElement.getElementsByTagName(a), d = [], e = 0;e < c.length;e++) {
    var f = c[e];
    f.hasAttribute(b) && d.push(f);
  }
  return d;
}
function insertPanelContents(a, b) {
  var c = createInnerHtmlElements(a.parentNode.tagName, b);
  a.parentNode.insertBefore(c, a);
}
function isComment(a) {
  return 8 == a.nodeType;
}
function getPanelStubs(a, b, c) {
  CHECK(a.parentNode == b.parentNode);
  for (var d = [];a != b.nextSibling;a = a.nextSibling) {
    isComment(a) && endsWith(a.data, PANEL_STUBEND + c) ? d.push(a) : a.tagName && a.firstChild && (d = d.concat(getPanelStubs(a.firstChild, a.lastChild, c)));
  }
  return d;
}
function insertStubAtIndex(a, b, c) {
  CHECK(a && 0 < b);
  b = a.children[b - 1] || null;
  var d = getDocument().createComment(PANEL_STUBSTART + c);
  a.insertBefore(d, b);
  c = getDocument().createComment(PANEL_STUBEND + c);
  a.insertBefore(c, b);
  return c;
}
function insertMissingStubUsingXpath(a, b) {
  for (var c = a.split("/"), d = /(?:.*\[@id=")(.*)(?:"\])/, e = /(?:.*\[)(\d+)(?:\])/, f = getDocument().body, g = 2;g < c.length;++g) {
    var h = d.exec(c[g]);
    if (h) {
      f = getDocument().getElementById(h[1]), CHECK(f);
    } else {
      if (h = e.exec(c[g])) {
        if (g == c.length - 1) {
          return insertStubAtIndex(f, Number(h[1]), b);
        }
        f = f.children[h[1] - 1];
        CHECK(f);
      } else {
        CHECK(0);
      }
    }
  }
}
function endsWith(a, b) {
  return(new RegExp(b + "$")).test(a);
}
function instantiateChildPanels(a, b, c) {
  for (var d in c) {
    if (c.hasOwnProperty(d) && d != XPATH && d != DONT_BIND && d != INSTANCE_HTML && d != IMAGES && d != CONTIGUOUS) {
      var e = c[d], f = getPanelStubs(a, b, d);
      CHECK(!e.length || !e[0][CONTIGUOUS]);
      for (var g = 0, h = 0;g < e.length;g++) {
        0 < g && !e[g][CONTIGUOUS] && h++, instantiatePanel(f[h], e[g], d);
      }
    }
  }
}
function instantiatePanel(a, b, c) {
  if (b && !b[DONT_BIND]) {
    if (a || (CHECK(b[XPATH]), a = insertMissingStubUsingXpath(b[XPATH], c), CHECK(a)), b[INSTANCE_HTML]) {
      c = createInnerHtmlElements(a.parentNode.tagName, b[INSTANCE_HTML]);
      var d = c.firstChild, e = c.lastChild;
      a.parentNode.insertBefore(c, a);
      instantiateChildPanels(d, e, b);
    } else {
      instantiateChildPanels(a.parentNode, a.parentNode, b);
    }
  }
}
function instantiatePanelsInPage(a) {
  var b = getDocument().documentElement;
  instantiateChildPanels(b.firstChild, b.lastChild, a);
  return getElementsByTagAndAttribute("img", BLINK_SRC);
}
function collectCriticalImages(a) {
  var b = {};
  if (!a) {
    return b;
  }
  for (var c = 0;c < a.length;c++) {
    var d = a[c], e = d.getAttribute(BLINK_SRC);
    e && (void 0 == b[e] && (b[e] = []), b[e].push(d));
  }
  return b;
}
function PageManager() {
}
PageManager.prototype.instantiatePage = function(a) {
  a = instantiatePanelsInPage(a);
  return collectCriticalImages(a);
};
})();
