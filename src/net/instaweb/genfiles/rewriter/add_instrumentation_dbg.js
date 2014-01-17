(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.AddInstrumentation = function(beaconUrlPrefix, event, extraParams, htmlUrl) {
  this.beaconUrlPrefix_ = beaconUrlPrefix;
  this.event_ = event;
  this.extraParams_ = extraParams;
  this.htmlUrl_ = htmlUrl;
};
pagespeed.beaconUrl = "";
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var url = this.beaconUrlPrefix_, oldStartTime = window.mod_pagespeed_start, traditionalPLT = Number(new Date) - oldStartTime, url = url + (-1 == url.indexOf("?") ? "?" : "&"), url = url + "ets=" + ("load" == this.event_ ? "load:" : "unload:"), url = url + traditionalPLT;
  if ("beforeunload" != this.event_ || !window.mod_pagespeed_loaded) {
    url += "&r" + this.event_ + "=";
    if (window.performance) {
      var timingApi = window.performance.timing, navStartTime = timingApi.navigationStart, requestStartTime = timingApi.requestStart, url = url + (timingApi[this.event_ + "EventStart"] - navStartTime), url = url + ("&nav=" + (timingApi.fetchStart - navStartTime)), url = url + ("&dns=" + (timingApi.domainLookupEnd - timingApi.domainLookupStart)), url = url + ("&connect=" + (timingApi.connectEnd - timingApi.connectStart)), url = url + ("&req_start=" + (requestStartTime - navStartTime)), url = url + 
      ("&ttfb=" + (timingApi.responseStart - requestStartTime)), url = url + ("&dwld=" + (timingApi.responseEnd - timingApi.responseStart)), url = url + ("&dom_c=" + (timingApi.domContentLoadedEventStart - navStartTime));
      window.performance.navigation && (url += "&nt=" + window.performance.navigation.type);
      var firstPaintTime = -1;
      timingApi.msFirstPaint ? firstPaintTime = timingApi.msFirstPaint : window.chrome && window.chrome.loadTimes && (firstPaintTime = Math.floor(1E3 * window.chrome.loadTimes().firstPaintTime));
      firstPaintTime -= requestStartTime;
      0 <= firstPaintTime && (url += "&fp=" + firstPaintTime);
    } else {
      url += traditionalPLT;
    }
    pagespeed.getResourceTimingData && window.parent == window && (url += pagespeed.getResourceTimingData());
    url += window.parent != window ? "&ifr=1" : "&ifr=0";
    if ("load" == this.event_) {
      window.mod_pagespeed_loaded = !0;
      var numPrefetchedResources = window.mod_pagespeed_num_resources_prefetched;
      numPrefetchedResources && (url += "&nrp=" + numPrefetchedResources);
      var prefetchStartTime = window.mod_pagespeed_prefetch_start;
      prefetchStartTime && (url += "&htmlAt=" + (oldStartTime - prefetchStartTime));
    }
    if (pagespeed.panelLoader) {
      var bcsi = pagespeed.panelLoader.getCsiTimingsString();
      "" != bcsi && (url += "&b_csi=" + bcsi);
    }
    if (pagespeed.criticalCss) {
      var cc = pagespeed.criticalCss, url = url + ("&ccis=" + cc.total_critical_inlined_size + "&cces=" + cc.total_original_external_size + "&ccos=" + cc.total_overhead_size + "&ccrl=" + cc.num_replaced_links + "&ccul=" + cc.num_unreplaced_links)
    }
    "" != this.extraParams_ && (url += this.extraParams_);
    document.referrer && (url += "&ref=" + encodeURIComponent(document.referrer));
    url += "&url=" + encodeURIComponent(this.htmlUrl_);
    pagespeed.beaconUrl = url;
    (new Image).src = url;
  }
};
pagespeed.addInstrumentationInit = function(beaconUrl, event, extraParams, htmlUrl) {
  var temp = new pagespeed.AddInstrumentation(beaconUrl, event, extraParams, htmlUrl);
  window.addEventListener ? window.addEventListener(event, function() {
    temp.sendBeacon();
  }, !1) : window.attachEvent("on" + event, function() {
    temp.sendBeacon();
  });
};
pagespeed.addInstrumentationInit = pagespeed.addInstrumentationInit;
})();
