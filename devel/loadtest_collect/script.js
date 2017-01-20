// This is basically just the stripped down http://phantomjs.org/quick-start.html
// example.
var page = require('webpage').create();
var system = require('system');
if (system.args.length === 1) {
  console.log('Usage: script.js <some URL>');
  phantom.exit();
}

page.open(system.args[1], function(status) {
  phantom.exit();
});
