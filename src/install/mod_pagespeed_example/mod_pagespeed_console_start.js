if (chartsOfflineCSS && chartsOfflineJS) {
  var numReady = 0;
  var init = function() {
    numReady++;
    if (numReady == 2) {
      pagespeed.initConsole();
    }
  };
  var scriptElem = document.createElement('script');
  scriptElem.setAttribute('src', chartsOfflineJS);
  scriptElem.onload = init;
  document.getElementsByTagName('head')[0].appendChild(scriptElem);
  var linkElem = document.createElement('link');
  linkElem.setAttribute('href', chartsOfflineCSS);
  linkElem.setAttribute('rel', 'stylesheet');
  linkElem.onload = init;
  document.getElementsByTagName('head')[0].appendChild(linkElem);
} else {
  google.load('visualization', '1.0', {'packages': ['corechart']});
  google.setOnLoadCallback(function() {
    pagespeed.initConsole();
  });
}
