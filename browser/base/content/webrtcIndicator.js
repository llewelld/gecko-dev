/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/webrtcUI.jsm");

const BUNDLE_URL = "chrome://browser/locale/webrtcIndicator.properties";
let gStringBundle;

function init(event) {
  gStringBundle = Services.strings.createBundle(BUNDLE_URL);

  let brand = Services.strings.createBundle("chrome://branding/locale/brand.properties");
  let brandShortName = brand.GetStringFromName("brandShortName");
  document.title =
    gStringBundle.formatStringFromName("webrtcIndicator.windowtitle",
                                       [brandShortName], 1);

  for (let id of ["audioVideoButton", "screenSharePopup"]) {
    let popup = document.getElementById(id);
    popup.addEventListener("popupshowing", onPopupMenuShowing);
    popup.addEventListener("popuphiding", onPopupMenuHiding);
    popup.addEventListener("command", onPopupMenuCommand);
  }

  let fxButton = document.getElementById("firefoxButton");
  fxButton.addEventListener("click", onFirefoxButtonClick);
  fxButton.addEventListener("mousedown", PositionHandler);

  updateIndicatorState();
}

function updateIndicatorState() {
  updateWindowAttr("sharingvideo", webrtcUI.showCameraIndicator);
  updateWindowAttr("sharingaudio", webrtcUI.showMicrophoneIndicator);
  updateWindowAttr("sharingscreen", webrtcUI.showScreenSharingIndicator);

  // Camera and microphone button tooltip.
  let shareTypes = [];
  if (webrtcUI.showCameraIndicator)
    shareTypes.push("Camera");
  if (webrtcUI.showMicrophoneIndicator)
    shareTypes.push("Microphone");

  let audioVideoButton = document.getElementById("audioVideoButton");
  if (shareTypes.length) {
    let stringId = "webrtcIndicator.sharing" + shareTypes.join("And") + ".tooltip";
    audioVideoButton.setAttribute("tooltiptext",
                                   gStringBundle.GetStringFromName(stringId));
  }
  else {
    audioVideoButton.removeAttribute("tooltiptext");
  }

  // Screen sharing button tooltip.
  let screenShareButton = document.getElementById("screenShareButton");
  if (webrtcUI.showScreenSharingIndicator) {
    let stringId = "webrtcIndicator.sharing" +
                   webrtcUI.showScreenSharingIndicator + ".tooltip";
    screenShareButton.setAttribute("tooltiptext",
                                    gStringBundle.GetStringFromName(stringId));
  }
  else {
    screenShareButton.removeAttribute("tooltiptext");
  }

  // Resize and ensure the window position is correct
  // (sizeToContent messes with our position).
  window.sizeToContent();
  PositionHandler.adjustPosition();
}

function updateWindowAttr(attr, value) {
  let docEl = document.documentElement;
  if (value)
    docEl.setAttribute(attr, "true");
  else
    docEl.removeAttribute(attr);
}

function onPopupMenuShowing(event) {
  let popup = event.target;
  let type = popup.getAttribute("type");

  let activeStreams;
  if (type == "Devices")
    activeStreams = webrtcUI.getActiveStreams(true, true, false);
  else
    activeStreams = webrtcUI.getActiveStreams(false, false, true);

  if (activeStreams.length == 1) {
    webrtcUI.showSharingDoorhanger(activeStreams[0], type);
    event.preventDefault();
    return;
  }

  for (let stream of activeStreams) {
    let item = document.createElement("menuitem");
    item.setAttribute("label", stream.browser.contentTitle || stream.uri);
    item.setAttribute("tooltiptext", stream.uri);
    item.stream = stream;
    popup.appendChild(item);
  }
}

function onPopupMenuHiding(event) {
  let popup = event.target;
  while (popup.firstChild)
    popup.firstChild.remove();
}

function onPopupMenuCommand(event) {
  let item = event.target;
  webrtcUI.showSharingDoorhanger(item.stream,
                                 item.parentNode.getAttribute("type"));
}

function onFirefoxButtonClick(event) {
  event.target.blur();
  let activeStreams = webrtcUI.getActiveStreams(true, true, true);
  activeStreams[0].browser.ownerDocument.defaultView.focus();
}

let PositionHandler = {
  positionCustomized: false,
  threshold: 10,
  adjustPosition: function() {
    if (!this.positionCustomized) {
      // Center the window horizontally on the screen (not the available area).
      // Until we have moved the window to y=0, 'screen.width' may give a value
      // for a secondary screen, so use values from the screen manager instead.
      let width = {};
      Cc["@mozilla.org/gfx/screenmanager;1"].getService(Ci.nsIScreenManager)
        .primaryScreen.GetRectDisplayPix({}, {}, width, {});
      window.moveTo((width.value - document.documentElement.clientWidth) / 2, 0);
    } else {
      // This will ensure we're at y=0.
      this.setXPosition(window.screenX);
    }
  },
  setXPosition: function(desiredX) {
    // Ensure the indicator isn't moved outside the available area of the screen.
    let desiredX = Math.max(desiredX, screen.availLeft);
    let maxX =
      screen.availLeft + screen.availWidth - document.documentElement.clientWidth;
    window.moveTo(Math.min(desiredX, maxX), 0);
  },
  handleEvent: function(aEvent) {
    switch (aEvent.type) {
      case "mousedown":
        if (aEvent.button != 0 || aEvent.defaultPrevented)
          return;

        this._startMouseX = aEvent.screenX;
        this._startWindowX = window.screenX;
        this._deltaX = this._startMouseX - this._startWindowX;

        window.addEventListener("mousemove", this);
        window.addEventListener("mouseup", this);
        break;

      case "mousemove":
        let moveOffset = Math.abs(aEvent.screenX - this._startMouseX);
        if (this._dragFullyStarted || moveOffset > this.threshold) {
          this.setXPosition(aEvent.screenX - this._deltaX);
          this._dragFullyStarted = true;
        }
        break;

      case "mouseup":
        this._dragFullyStarted = false;
        window.removeEventListener("mousemove", this);
        window.removeEventListener("mouseup", this);
        this.positionCustomized =
          Math.abs(this._startWindowX - window.screenX) >= this.threshold;
        break;
    }
  }
};
