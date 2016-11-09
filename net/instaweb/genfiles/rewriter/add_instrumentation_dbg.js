(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.AddInstrumentation = function(a, c, b, d) {
  this.beaconUrlPrefix_ = a;
  this.event_ = c;
  this.extraParams_ = b;
  this.htmlUrl_ = d;
};
pagespeed.beaconUrl = "";
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var a = this.beaconUrlPrefix_, c = window.mod_pagespeed_start, b = Number(new Date) - c, a = a + (-1 == a.indexOf("?") ? "?" : "&"), a = a + "ets=" + ("load" == this.event_ ? "load:" : "unload:");
  if ("beforeunload" != this.event_ || !window.mod_pagespeed_loaded) {
    a = a + b + ("&r" + this.event_ + "=");
    if (window.performance) {
      var b = window.performance.timing, d = b.navigationStart, e = b.requestStart, a = a + (b[this.event_ + "EventStart"] - d), a = a + ("&nav=" + (b.fetchStart - d)), a = a + ("&dns=" + (b.domainLookupEnd - b.domainLookupStart)), a = a + ("&connect=" + (b.connectEnd - b.connectStart)), a = a + ("&req_start=" + (e - d)) + ("&ttfb=" + (b.responseStart - e)), a = a + ("&dwld=" + (b.responseEnd - b.responseStart)), a = a + ("&dom_c=" + (b.domContentLoadedEventStart - d));
      window.performance.navigation && (a += "&nt=" + window.performance.navigation.type);
      d = -1;
      b.msFirstPaint ? d = b.msFirstPaint : window.chrome && window.chrome.loadTimes && (d = Math.floor(1E3 * window.chrome.loadTimes().firstPaintTime));
      d -= e;
      0 <= d && (a += "&fp=" + d);
    } else {
      a += b;
    }
    pagespeed.getResourceTimingData && window.parent == window && (a += pagespeed.getResourceTimingData());
    a += window.parent != window ? "&ifr=1" : "&ifr=0";
    "load" == this.event_ && (window.mod_pagespeed_loaded = !0, (b = window.mod_pagespeed_num_resources_prefetched) && (a += "&nrp=" + b), (b = window.mod_pagespeed_prefetch_start) && (a += "&htmlAt=" + (c - b)));
    pagespeed.criticalCss && (c = pagespeed.criticalCss, a += "&ccis=" + c.total_critical_inlined_size + "&cces=" + c.total_original_external_size + "&ccos=" + c.total_overhead_size + "&ccrl=" + c.num_replaced_links + "&ccul=" + c.num_unreplaced_links);
    a += "&dpr=" + window.devicePixelRatio;
    "" != this.extraParams_ && (a += this.extraParams_);
    document.referrer && (a += "&ref=" + encodeURIComponent(document.referrer));
    a += "&url=" + encodeURIComponent(this.htmlUrl_);
    pagespeed.beaconUrl = a;
    (new Image).src = a;
  }
};
pagespeed.addInstrumentationInit = function(a, c, b, d) {
  var e = new pagespeed.AddInstrumentation(a, c, b, d);
  window.addEventListener ? window.addEventListener(c, function() {
    e.sendBeacon();
  }, !1) : window.attachEvent("on" + c, function() {
    e.sendBeacon();
  });
};
pagespeed.addInstrumentationInit = pagespeed.addInstrumentationInit;
})();
