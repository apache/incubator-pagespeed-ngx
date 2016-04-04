(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.AddInstrumentation = function(a, d, b, c) {
  this.beaconUrlPrefix_ = a;
  this.event_ = d;
  this.extraParams_ = b;
  this.htmlUrl_ = c;
};
pagespeed.beaconUrl = "";
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var a = this.beaconUrlPrefix_, d = window.mod_pagespeed_start, b = Number(new Date) - d, a = a + (-1 == a.indexOf("?") ? "?" : "&"), a = a + "ets=" + ("load" == this.event_ ? "load:" : "unload:"), a = a + b;
  if ("beforeunload" != this.event_ || !window.mod_pagespeed_loaded) {
    a += "&r" + this.event_ + "=";
    if (window.performance) {
      var b = window.performance.timing, c = b.navigationStart, e = b.requestStart, a = a + (b[this.event_ + "EventStart"] - c), a = a + ("&nav=" + (b.fetchStart - c)), a = a + ("&dns=" + (b.domainLookupEnd - b.domainLookupStart)), a = a + ("&connect=" + (b.connectEnd - b.connectStart)), a = a + ("&req_start=" + (e - c)), a = a + ("&ttfb=" + (b.responseStart - e)), a = a + ("&dwld=" + (b.responseEnd - b.responseStart)), a = a + ("&dom_c=" + (b.domContentLoadedEventStart - c));
      window.performance.navigation && (a += "&nt=" + window.performance.navigation.type);
      c = -1;
      b.msFirstPaint ? c = b.msFirstPaint : window.chrome && window.chrome.loadTimes && (c = Math.floor(1E3 * window.chrome.loadTimes().firstPaintTime));
      c -= e;
      0 <= c && (a += "&fp=" + c);
    } else {
      a += b;
    }
    pagespeed.getResourceTimingData && window.parent == window && (a += pagespeed.getResourceTimingData());
    a += window.parent != window ? "&ifr=1" : "&ifr=0";
    "load" == this.event_ && (window.mod_pagespeed_loaded = !0, (b = window.mod_pagespeed_num_resources_prefetched) && (a += "&nrp=" + b), (b = window.mod_pagespeed_prefetch_start) && (a += "&htmlAt=" + (d - b)));
    pagespeed.criticalCss && (d = pagespeed.criticalCss, a += "&ccis=" + d.total_critical_inlined_size + "&cces=" + d.total_original_external_size + "&ccos=" + d.total_overhead_size + "&ccrl=" + d.num_replaced_links + "&ccul=" + d.num_unreplaced_links);
    a += "&dpr=" + window.devicePixelRatio;
    "" != this.extraParams_ && (a += this.extraParams_);
    document.referrer && (a += "&ref=" + encodeURIComponent(document.referrer));
    a += "&url=" + encodeURIComponent(this.htmlUrl_);
    pagespeed.beaconUrl = a;
    (new Image).src = a;
  }
};
pagespeed.addInstrumentationInit = function(a, d, b, c) {
  var e = new pagespeed.AddInstrumentation(a, d, b, c);
  window.addEventListener ? window.addEventListener(d, function() {
    e.sendBeacon();
  }, !1) : window.attachEvent("on" + d, function() {
    e.sendBeacon();
  });
};
pagespeed.addInstrumentationInit = pagespeed.addInstrumentationInit;
})();
