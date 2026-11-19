/* Copyright 2021-2026 Paulo Mateus -- SPDX-License-Identifier: Apache-2.0

   "Install on my MOD": pushes the bundle of a plugin into a MOD unit plugged in over USB, from the browser.

   It speaks the protocol of the unit's mod-ui, the one the MOD Cloud Builder (github.com/mod-audio/mod-cloud-builder) uses:
     ws://192.168.51.1/rplsocket   (the unit's address on the USB network, mod-ui 1.13.3 or newer)
     1. send ''                    -> {"platform": "dwarf" | "duox" | "duo", "version": ..., "bin-compat": ...}
     2. send base64(<bundle>.tar.gz) -> {"ok": true, "installed": [...]} or {"ok": false, "error": "..."}
   mod-ui unpacks the archive, installs the bundles it finds and tells every open MOD UI to rescan its plugins.

   Two things the page cannot change:
     - mod-ui accepts the socket only from pages of *.mod.audio and localhost:8010 (`check_origin`): from any other
       origin the handshake is answered with 403, which a page sees only as "could not connect". It says so before trying.
     - Chromium asks once for the permission to reach the local network; Firefox and Safari refuse ws:// from an https page.

   The data (which bundle for which platform) is in <script id="install-data"> and written by tools/site/build_site.py. */
(function () {
  'use strict';

  var DEVICE_URL = 'ws://192.168.51.1/rplsocket';
  var NAMES = {dwarf: 'MOD Dwarf', duox: 'MOD Duo X', duo: 'MOD Duo'};
  var STEPS = ['Connect to the MOD', 'Download the plugin', 'Send it to the MOD'];

  var dataElement = document.getElementById('install-data');
  var dialog = document.getElementById('install');
  if (!dataElement || !dialog || typeof dialog.showModal !== 'function') {
    // No data (a site built without bundles) or a browser without <dialog>: the buttons stay hidden
    return;
  }
  var data = JSON.parse(dataElement.textContent);
  var $ = function (selector) { return dialog.querySelector(selector); };
  var steps = $('.install-steps');
  var status = $('.install-status');
  var hint = $('.install-hint');
  var go = $('.install-go');
  var again = $('.install-again');

  var session = null;     // {plugin, ws, platform, state: 'connecting' | 'ready' | 'installing' | 'done' | 'failed'}

  function isChromium() { return /\bChrome\/\d+/.test(navigator.userAgent); }

  // What mod-ui's check_origin accepts: the same test, to say so before the unit answers with a bare 403
  function originAccepted() {
    var host = location.host;
    return (location.protocol === 'http:' || location.protocol === 'https:')
      && (host === 'localhost:8010' || host === 'mod.audio' || /\.mod\.audio$/.test(host));
  }

  function setStep(index, state) {
    var items = steps.children;
    for (var i = 0; i < items.length; i++) {
      if (i < index) { items[i].dataset.state = 'done'; }
      else if (i === index) { items[i].dataset.state = state; }
      else { items[i].dataset.state = 'todo'; }
    }
  }

  function say(text, kind) {
    status.textContent = text;
    status.dataset.kind = kind || '';
  }

  function paragraphs(list) {
    hint.textContent = '';
    list.forEach(function (text) {
      var p = document.createElement('p');
      p.textContent = text;
      hint.appendChild(p);
    });
    hint.hidden = list.length === 0;
  }

  function fail(index, text, hints) {
    if (session) { session.state = 'failed'; }
    setStep(index, 'failed');
    say(text, 'error');
    paragraphs(hints || []);
    go.hidden = true;
    again.hidden = false;
    again.focus();
  }

  // Why the unit could not be reached, most certain cause first
  function connectionHints() {
    var list = [];
    if (!originAccepted()) {
      list.push('This page is at ' + location.host + '. The mod-ui of the unit only accepts connections from pages of '
        + 'mod.audio sites and from localhost:8010, so it refuses this one before anything is sent. Open the site from one of those addresses.');
    }
    if (location.protocol === 'https:' && !isChromium()) {
      list.push('Firefox and Safari do not let an https page talk to a device on your network. Use Chrome, Edge or another Chromium browser.');
    } else if (isChromium()) {
      list.push('If the browser asked for permission to access devices on your local network, allow it and press Connect again.');
    }
    list.push('Check that the MOD is plugged into this computer with its USB cable, turned on, and running mod-ui 1.13.3 or newer (' + DEVICE_URL + ').');
    return list;
  }

  function closeSocket() {
    if (session && session.ws) {
      var ws = session.ws;
      session.ws = null;
      try { ws.close(); } catch (e) { /* already closed */ }
    }
  }

  function connect() {
    closeSocket();
    var current = session;
    current.state = 'connecting';
    current.platform = null;
    setStep(0, 'doing');
    paragraphs([]);
    again.hidden = true;
    go.hidden = false;
    go.disabled = true;
    say('Connecting to the MOD…');
    if (!originAccepted()) {
      paragraphs(connectionHints().slice(0, 1));
    }

    var ws;
    try {
      ws = new WebSocket(DEVICE_URL);
    } catch (e) {
      fail(0, 'The browser refused to open the connection.', connectionHints());
      return;
    }
    current.ws = ws;
    var waiting = setTimeout(function () {
      if (session === current && current.ws === ws && ws.readyState === WebSocket.CONNECTING && isChromium()) {
        say('Waiting for the permission to access devices on your local network…');
      }
    }, 1500);

    ws.onopen = function () {
      if (current.ws === ws) { ws.send(''); }
    };
    ws.onerror = function () { /* onclose follows, and says what happened */ };
    ws.onclose = function () {
      clearTimeout(waiting);
      if (session !== current || current.ws !== ws) { return; }
      current.ws = null;
      if (current.state === 'connecting') {
        fail(0, 'Could not connect to the MOD.', connectionHints());
      } else if (current.state === 'ready' || current.state === 'installing') {
        fail(current.state === 'ready' ? 0 : 2, 'The connection to the MOD was closed.', ['Is the USB cable still plugged in?']);
      }
    };
    ws.onmessage = function (event) {
      if (current.ws !== ws) { return; }
      var message;
      try { message = JSON.parse(event.data); } catch (e) { return; }
      if (current.state === 'connecting') {
        clearTimeout(waiting);
        gotDevice(current, message);
      } else if (current.state === 'installing') {
        gotResult(current, message);
      }
    };
  }

  function gotDevice(current, info) {
    var platform = String(info.platform || '');
    var file = current.plugin.files[platform];
    current.platform = platform;
    current.file = file;
    var name = NAMES[platform] || 'a MOD unit (' + platform + ')';
    if (!file) {
      fail(0, 'Connected to ' + name + ', but this site has no build of ' + current.plugin.title + ' for it.',
        ['Builds available: ' + Object.keys(current.plugin.files).map(function (p) { return NAMES[p] || p; }).join(', ') + '.']);
      return;
    }
    current.state = 'ready';
    setStep(1, 'todo');
    say('Connected to ' + name + (info.version ? ' (' + info.version + ')' : '') + '. Ready to install ' + current.plugin.title + '.', 'ok');
    paragraphs(['Build ' + data.build.sha + ' of ' + data.build.ref + ', ' + data.build.date + '.']);
    go.disabled = false;
    go.focus();
  }

  function toBase64(blob) {
    return new Promise(function (resolve, reject) {
      var reader = new FileReader();
      reader.onload = function () { resolve(String(reader.result).split(',')[1] || ''); };
      reader.onerror = function () { reject(reader.error); };
      reader.readAsDataURL(blob);
    });
  }

  function install() {
    var current = session;
    if (!current || current.state !== 'ready') { return; }
    current.state = 'installing';
    go.disabled = true;
    paragraphs([]);
    setStep(1, 'doing');
    say('Downloading ' + current.plugin.title + '…');
    fetch(current.file.url)
      .then(function (response) {
        if (!response.ok) { throw new Error('HTTP ' + response.status); }
        return response.blob();
      })
      .then(toBase64)
      .then(function (base64) {
        if (session !== current || !current.ws) { return; }
        setStep(2, 'doing');
        say('Sending ' + current.plugin.title + ' to the MOD (' + Math.round(current.file.size / 1024) + ' KB)…');
        current.ws.send(base64);
      })
      .catch(function (error) {
        if (session === current && current.state === 'installing') {
          fail(1, 'Could not download the plugin: ' + error.message + '.', ['Reload the page and try again.']);
        }
      });
  }

  function gotResult(current, result) {
    if (result.ok) {
      current.state = 'done';
      setStep(3, 'done');
      say(current.plugin.title + ' is installed.', 'ok');
      paragraphs(['It is in the list of plugins of MOD UI now. A pedalboard that was already open may need to be reloaded to use the new version.']);
      go.hidden = true;
      again.hidden = true;
    } else {
      fail(2, 'The MOD did not install it: ' + (result.error || 'unknown error') + '.', []);
    }
  }

  function open(name) {
    var plugin = data.plugins[name];
    if (!plugin) { return; }
    session = {plugin: plugin, ws: null, state: 'connecting'};
    $('.install-title').textContent = 'Install ' + plugin.title;
    steps.textContent = '';
    STEPS.forEach(function (text) {
      var li = document.createElement('li');
      li.textContent = text;
      li.dataset.state = 'todo';
      steps.appendChild(li);
    });
    dialog.showModal();
    connect();
  }

  go.addEventListener('click', install);
  again.addEventListener('click', connect);
  dialog.addEventListener('close', function () { closeSocket(); session = null; });
  document.addEventListener('click', function (event) {
    var button = event.target.closest && event.target.closest('[data-install]');
    if (button) { open(button.dataset.install); }
  });
  document.documentElement.dataset.installReady = 'on';
})();
