// Copyright 2011 Google Inc. All Rights Reserved.

/**
 * @fileoverview Implements Layout and Panels for Blink client
 * Binds JSON data to layout panels in javascript.
 * An HTML element may have a panelId specified.
 * This id identifies a JSON paneldata element to bind to which may be an array
 * or a dictionary.
 * @author ssundaram@google.com (Sridhar Sundaram)
 */

var PANEL_ID = 'panel-id';
var PANEL_STUBSTART = 'GooglePanel begin ';
var PANEL_STUBEND = 'GooglePanel end ';
var INSTANCE_HTML = 'instance_html';
var CONTIGUOUS = 'contiguous';
var XPATH = 'xpath';
var DONT_BIND = 'dont_bind';
var IMAGES = 'images';
var BLINK_SRC = 'pagespeed_high_res_src';
var PANEL_MARKER = 'psa_disabled';

// TODO(ksimbili): Convert this to DCHECK using some flag
function CHECK(condition) {
  if (!condition) throw ('CHECK failed');
}

var getDocument = function() {
  return document;
};

function isInternetExplorer() {
  return navigator.appName == 'Microsoft Internet Explorer';
}

/**
 * Create an HTML dom element with tagName and innerHTML as specified and
 * return the nodes corresponding to innerHTML in a document fragment
 * @param {string} tagName
 * @param {string} innerHTML
 * Precondition: innerHTML is consistent with tagName.
 * @return {Element}
 */
function createInnerHtmlElements(tagName, innerHTML) {
  if (tagName == 'HEAD') tagName = 'div'; // innerHTML not allowed in HEAD in IE
  var element;
  if (isInternetExplorer() && (tagName == 'TABLE' || tagName == 'TBODY')) {
    element = getDocument().createElement('div');
    element.innerHTML = '<table>' + innerHTML + '</table>';
    element = element.getElementsByTagName('tbody')[0];
  } else {
    element = getDocument().createElement(tagName);
    element.innerHTML = innerHTML;
  }
  // Transfer all instances over to a document fragment
  var docFragment = getDocument().createDocumentFragment();
  while (element.childNodes.length > 0) {
    docFragment.appendChild(element.childNodes[0]);
  }
  return docFragment;
}

/**
 * Returns ordered array of nodes which match the xpath in context of node.
 * @param {Node} node - node within whose context xpath is found.
 * @param {string} xpath - xpath.
 * @return {Array.<Node>}
 */
function getMatchingXPathInDom(node, xpath) {
  var xpathResults =
      getDocument().evaluate(xpath, node, null,
                             XPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
  var results = [];
  for (var result; result = xpathResults.iterateNext(); results.push(result)) {}
  return results;
}

/**
 * Find all elements in the document which have tagName as tag and
 * have an attribute with name attributeName.
 * @param {string} tagName
 * @param {string} attributeName
 * @return {Array.<Node>} matching elements in the document.
 */
function getElementsByTagAndAttribute(tagName, attributeName) {
  var elements = getDocument().documentElement.getElementsByTagName(tagName);
  var elementsWithAttribute = [];
  for (var i = 0; i < elements.length; i++) {
    var element = elements[i];
    if (element.hasAttribute(attributeName)) {
      elementsWithAttribute.push(element);
    }
  }
  return elementsWithAttribute;
}

/**
 * Insert panel outerHTML above the panel Stub.
 * @param {Element} panelStub dom element indicating panel location.
 * @param {string} panelInstanceHtml outer html corresponding to panel content.
 */
function insertPanelContents(panelStub, panelInstanceHtml) {
  var panelHtmlNodes =
    createInnerHtmlElements(panelStub.parentNode.tagName, panelInstanceHtml);
  panelStub.parentNode.insertBefore(panelHtmlNodes, panelStub);
}

function isComment(node) {
  return node.nodeType == 8; // node.COMMENT_NODE is not defined in IE8
}

/**
 * Finds and returns the panel stubs within the range of nodes between
 * [beginNode,endNode] both inclusive, corresponding to panelId.
 * panel stub is a descendant of one of the nodes from
 *     begin to end nodes
 * @param {Node} beginNode - begin node range.
 * @param {Node} endNode - end node range.
 * @param {string} panelId - id of the panel whose stub is required.
 * @return {Array.<Node>}
 */
function getPanelStubs(beginNode, endNode, panelId) {
  CHECK(beginNode.parentNode == endNode.parentNode);
  var panelStubs = [];
  for (var node = beginNode; node != endNode.nextSibling;
       node = node.nextSibling) {
    if (isComment(node) && endsWith(node.data, PANEL_STUBEND + panelId)) {
      panelStubs.push(node);
    } else if (node.tagName && node.firstChild) {
      panelStubs = panelStubs.concat(
          getPanelStubs(node.firstChild, node.lastChild, panelId));
    }
  }
  return panelStubs;
}

/**
 * Inserts stubs at the given childIndex in given parentNode.
 * @param {Element} parentNode - Stub is inserted as child of this node.
 * @param {number} childIndex - Index at which stub is inserted.
 * @param {string} panelId - panelId of the stub.
 * @return {Node} - End stub corresponding to the panelId.
 */
function insertStubAtIndex(parentNode, childIndex, panelId) {
  CHECK(parentNode && childIndex > 0);
  var childNode = parentNode.children[childIndex - 1] || null;
  // Insert stubs and proceed as normal.
  var startStub = getDocument().createComment(PANEL_STUBSTART + panelId);
  parentNode.insertBefore(startStub, childNode);
  var endStub = getDocument().createComment(PANEL_STUBEND + panelId);
  parentNode.insertBefore(endStub, childNode);
  return endStub;
}

/**
 * Inserts missing stubs at the position specified by the xpath.
 * eg., //div[5]/span[6]/div[4]
 * @param {string} xpath - Xpath for specific location in the DOM.
 * @param {string} panelId - panelId of the stub.
 * @return {Node} - End stub corresponding to the panelId.
 */
function insertMissingStubUsingXpath(xpath, panelId) {
  var xpathUnits = xpath.split('/');
  var idRegExp = new RegExp('(?:.*\\[@id\=\")(.*)(?:\"\\])');
  var indexRegExp = new RegExp('(?:.*\\[)(\\d+)(?:\\])');
  // ignore first 2 entries.
  var parentNode = getDocument().body;
  for (var i = 2; i < xpathUnits.length; ++i) {
    var matches = idRegExp.exec(xpathUnits[i]);
    if (matches) {
      parentNode = getDocument().getElementById(matches[1]);
      CHECK(parentNode);
    } else {
      var indexMatch = indexRegExp.exec(xpathUnits[i]);
      if (indexMatch) {
        if (i == xpathUnits.length - 1) {
          return insertStubAtIndex(parentNode, Number(indexMatch[1]), panelId);
        }
        parentNode = parentNode.children[indexMatch[1] - 1];
        CHECK(parentNode);
      } else {
        CHECK(0);
      }
    }
  }
}

/**
 * Checks id the string ends with the given suffix.
 * @param {string} str - string to be used for comparison.
 * @param {string} suffix - suffix to be used for comparison.
 * @return {boolean} true - if str ends with suffix.
 */
function endsWith(str, suffix) {
  var re = new RegExp(suffix + '$');
  return re.test(str);
}

/**
 * Instantiate the child Panels for panelData in descendants of range of nodes
 * @param {Node} beginNode - begin range of nodes of panel instances
 *                           for which childpanels are being instantiated.
 * @param {Node} endNode - end range of nodes of panel instances
 *                         for which childpanels are being instantiated.
 * @param {Object.<string, string>} panelData contains
 *                     self for panel's own outerHTML if any (null, if none)
 *                     repeated childPanelId - panelData for child.
 */
function instantiateChildPanels(beginNode, endNode, panelData) {
  for (var childPanelId in panelData) {
    if (!panelData.hasOwnProperty(childPanelId) ||
            childPanelId == XPATH || childPanelId == DONT_BIND ||
            childPanelId == INSTANCE_HTML || childPanelId == IMAGES ||
            childPanelId == CONTIGUOUS) continue;
    var childPanelData = panelData[childPanelId];
    var childPanelStubs = getPanelStubs(beginNode, endNode, childPanelId);
    CHECK(!childPanelData.length || !childPanelData[0][CONTIGUOUS]);
    for (var i = 0, k = 0; i < childPanelData.length; i++) {
      // If contiguous, we can continue using the old stub.
      // check only with i>0 since k starts with 0 and first instance is always
      // false
      if (i > 0 && !childPanelData[i][CONTIGUOUS]) k++;
      instantiatePanel(childPanelStubs[k], childPanelData[i], childPanelId);
    }
  }
}
/**
 * Instantiate the panel node with its corresponding dictionary of data.
 * @param {Node} panelStub panel stub node.
 * @param {Object.<string, string>} panelData contains
 *                     self for panel's own outerHTML if any (null, if none)
 *                     repeated childPanelId - panelData for child.
 * @param {string} panelId Panel Id.
 */
function instantiatePanel(panelStub, panelData, panelId) {
  if (!panelData || panelData[DONT_BIND]) return;

  if (!panelStub) {
    CHECK(panelData[XPATH]);
    panelStub = insertMissingStubUsingXpath(panelData[XPATH], panelId);
    CHECK(panelStub);
  }
  // Insert instance HTML
  if (panelData[INSTANCE_HTML]) {
    var panelHtmlNodes = createInnerHtmlElements(
        panelStub.parentNode.tagName, panelData[INSTANCE_HTML]);
    var firstNode = panelHtmlNodes.firstChild;
    var lastNode = panelHtmlNodes.lastChild;
    panelStub.parentNode.insertBefore(panelHtmlNodes, panelStub);
    instantiateChildPanels(firstNode, lastNode, panelData);
  } else { // TODO(ssundaram): Navigate to correct range of nodes here.
    instantiateChildPanels(panelStub.parentNode, panelStub.parentNode,
                           panelData);
  }
}

/**
 * Instantiates layout with pageData to create HTML page. The layout has
 * top level panel stubs.
 * @param {Object.<string,string>} pageJsonDict - panelData corresponding to top
 *   level panels.
 * @return {Array.<Node>} array of image elements instantiated due to the
 *         insertion of panel HTML content.
 */
function instantiatePanelsInPage(pageJsonDict) {
  // Instantiate top level panels and collect their images
  var docElement = getDocument().documentElement;
  instantiateChildPanels(docElement.firstChild, docElement.lastChild,
      pageJsonDict);
  var criticalImages = getElementsByTagAndAttribute('img', BLINK_SRC);
  return criticalImages;
}

/**
 * Find all pushed image elements and collect in a dictionary
 * ASSUMPTION: Critical images are indicated by having BLINK_SRC attribute
 * @param {Array.<Node>} criticalImages Array of critical images.
 * @return {Object.<String,Array.<Element>>} dictionary mapping from
 *          url --> [image] for each. <image BLINK_SRC=url>
 */
function collectCriticalImages(criticalImages) {
  var criticalImagesByUrl = {};

  if (!criticalImages) return criticalImagesByUrl;

  for (var i = 0; i < criticalImages.length; i++) {
    var image = criticalImages[i];
    var url = image.getAttribute(BLINK_SRC);
    // The BLINK_SRC attribute is no longer required, but we will keep it.
    if (url) {
      if (criticalImagesByUrl[url] == undefined) {
        criticalImagesByUrl[url] = [];
      }
      criticalImagesByUrl[url].push(image);
    }
  }
  return criticalImagesByUrl;
}


/**
 * Page Manager for the layout
 * @constructor
 */
function PageManager() {
}

/**
 * Instantiates the layout with the jsonData for the page.
 * @param {Object.<string,string>} pageJsonData - panelData corresponding to top
 *   level panels.
 * @return {Object.<string,Array.<Element>>} dictionary mapping from
 *          url --> [image] for each. <image BLINK_SRC=url>
 */
PageManager.prototype.instantiatePage = function(pageJsonData) {
  var criticalImages = instantiatePanelsInPage(pageJsonData);
  return collectCriticalImages(criticalImages);
};
