(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.getResourceTimingData = function() {
  if (window.performance && (window.performance.getEntries || window.performance.webkitGetEntries)) {
    for (var totalFetchDuration = 0, maxFetchDuration = 0, numFetches = 0, totalDnsDuration = 0, numDnsLookups = 0, totalConnectionTime = 0, numConnections = 0, totalTTFB = 0, numTTFBRequests = 0, totalBlockingTime = 0, numRequestsBlocked = 0, entryCountMap = {}, entries = window.performance.getEntries ? window.performance.getEntries() : window.performance.webkitGetEntries(), i = 0;i < entries.length;i++) {
      var duration = entries[i].duration;
      0 < duration && (totalFetchDuration += duration, ++numFetches, maxFetchDuration = Math.max(maxFetchDuration, duration));
      var connectTime = entries[i].connectEnd - entries[i].connectStart;
      0 < connectTime && (totalConnectionTime += connectTime, ++numConnections);
      var dnsTime = entries[i].domainLookupEnd - entries[i].domainLookupStart;
      0 < dnsTime && (totalDnsDuration += dnsTime, ++numDnsLookups);
      var initiator = entries[i].initiatorType;
      entryCountMap[initiator] ? ++entryCountMap[initiator] : entryCountMap[initiator] = 1;
      var blockingTime = entries[i].requestStart - entries[i].fetchStart;
      0 < blockingTime && (totalBlockingTime += blockingTime, ++numRequestsBlocked);
      var ttfb = entries[i].responseStart - entries[i].requestStart;
      0 < ttfb && (totalTTFB += ttfb, ++numTTFBRequests);
    }
    return "&afd=" + (numFetches ? Math.round(totalFetchDuration / numFetches) : 0) + "&nfd=" + numFetches + "&mfd=" + Math.round(maxFetchDuration) + "&act=" + (numConnections ? Math.round(totalConnectionTime / numConnections) : 0) + "&nct=" + numConnections + "&adt=" + (numDnsLookups ? Math.round(totalDnsDuration / numDnsLookups) : 0) + "&ndt=" + numDnsLookups + "&abt=" + (numRequestsBlocked ? Math.round(totalBlockingTime / numRequestsBlocked) : 0) + "&nbt=" + numRequestsBlocked + "&attfb=" + (numTTFBRequests ? 
    Math.round(totalTTFB / numTTFBRequests) : 0) + "&nttfb=" + numTTFBRequests + (entryCountMap.css ? "&rit_css=" + entryCountMap.css : "") + (entryCountMap.link ? "&rit_link=" + entryCountMap.link : "") + (entryCountMap.script ? "&rit_script=" + entryCountMap.script : "") + (entryCountMap.img ? "&rit_img=" + entryCountMap.img : "");
  }
  return "";
};
pagespeed.getResourceTimingData = pagespeed.getResourceTimingData;
})();
