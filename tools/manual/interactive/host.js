/* Copyright 2021-2026 Paulo Mateus -- SPDX-License-Identifier: Apache-2.0

   The host of the interactive manual (docs/interactive-manuals.md), which runs INSIDE the frame of the manual. It plays the part
   of MOD UI for the REAL files of the plugin (its icon, its settings page, its stylesheet and its script, none of them
   copied or redrawn): it renders nothing by itself, it only
     * gives the script of the plugin what MOD UI gives it (`event`, `funcs`, a start event with the ports and the
       parameters, a stand-in for the host that answers the way mod-host does for a plugin that selects one preset at a time),
     * opens and closes the settings page like the window of MOD UI,
     * and offers the page around it (panel.js) a small API: show a view, tap or type with a finger, ring a part, outline what can be
       tapped, and say what the person tapped.

   There is no plugin behind it: ports and parameters only hold values, and what the interface asks the plugin to do is
   only kept in `Host.log`. Everything it draws on top is in `.ix-overlay`: nothing of it is part of the plugin. */
(function () {
  'use strict';

  var CFG = window.__IX__;
  var $ = window.jQuery;
  var method = window.__IX_METHOD__;
  var doc = document;
  var reduced = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  var sleep = function (ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); };

  window.PLATFORM = CFG.platform || 'dwarf';   // the global that MOD UI defines from the hardware descriptor of the device

  var Host = window.Host = {
    ready: false,
    log: [],
    speed: 1,                 // 2 plays twice as fast (the player of the tours)
    handlers: { select: null, view: null },
    areas: [],
    showing: false,
  };

  // ------------------------------------------------------------------------------------------ MOD UI's part of the page
  var board = $('.ix-board');
  var stage = $('.ix-stage');
  var win = $('.ix-window');
  var icon = stage.children('.mod-pedal');
  var settings = win.children('.mod-settings');
  var view = 'pedalboard';

  // What MOD UI adds around the icon (the gear above it) and to its ports
  $('<div class="mod-actions">').append($('<div class="mod-settings" role="button" tabindex="0" aria-label="Settings of the plugin" title="Settings">')).appendTo(icon);
  icon.find('[mod-role$="-port"]').each(function () {
    var match = /^(input|output)-(\w+)-port$/.exec($(this).attr('mod-role') || '');
    if (match) {
      $(this).addClass('mod-' + match[1] + ' mod-' + match[2] + '-' + match[1] + ' ' + match[1] + '-disconnected');
      if (match[1] === 'output') { $('<div class="mod-output-jack jack-disconnected">').appendTo(this); }
    }
  });

  // ------------------------------------------------------------------------------------------ ports, the script of the plugin
  var portValues = {};
  CFG.ports.forEach(function (port) { portValues[port.symbol] = port.default; });
  var parameterValues = Object.assign({}, CFG.state);
  var data = {};

  var funcs = {
    set_port_value: function (symbol, value) {
      // MOD UI sends nothing when the port already has that value (setPortValue: `port.value == value`)
      if (portValues[symbol] == value) { Host.log.push(['dropped by MOD UI (same value)', symbol, value]); return; }
      portValues[symbol] = value;
      updateMeters();
      Host.log.push(['set_port_value', symbol, value]);
      var selector = /^Preset_selector_(\d+)$/.exec(symbol);
      if (simulate && selector && value == 1) { setTimeout(function () { hostSelects(Number(selector[1]), symbol); }, latency()); }
      if (simulate && symbol === 'Preset_combobox' && !('Active_preset' in portValues)) { setTimeout(function () { hostSelects(Math.round(value) + 1, symbol); }, latency()); }
    },
    // The plugin answers a patch:Get with a 'change' event that has a uri
    patch_get: function (uri) { Host.log.push(['patch_get', uri]); setTimeout(function () { send({ type: 'change', uri: uri, value: parameterValues[uri] }); }, 0); },
    patch_set: function (uri, type, value) { parameterValues[uri] = value; Host.log.push(['patch_set', uri, type, value]); },
    get_custom_resource_filename: function (file) { return file; },
    get_port_index_for_symbol: function (symbol) {
      var port = CFG.ports.filter(function (p) { return p.symbol === symbol; })[0];
      return port ? port.index : -1;
    },
  };

  function send(event) {
    if (event.symbol !== undefined) { portValues[event.symbol] = event.value; }
    updateMeters();
    method(Object.assign({ data: data, icon: icon, settings: settings, api_version: 3 }, event), funcs);
  }
  Host.send = send;

  // A stand-in for MOD UI and mod-host, for plugins that select one preset at a time with `Preset_selector_N` ports: the plugin
  // turns the previous selector off, the new one on and moves `Preset_combobox`, and, like mod-host, leaves out the port the
  // change came through (the host already holds that value)
  var simulate = 'Preset_selector_1' in portValues;
  var latency = function () { return 150 / Host.speed; };
  var hostPreset = 1;
  while (hostPreset < 64 && portValues['Preset_selector_' + hostPreset] !== 1) { hostPreset++; }
  function hostSelects(preset, through) {
    var previous = hostPreset;
    hostPreset = preset;
    var answer = function (symbol, value) { if (symbol !== through) { send({ type: 'change', symbol: symbol, value: value }); } };
    if (previous !== preset) { answer('Preset_selector_' + previous, 0); }
    answer('Preset_selector_' + preset, 1);
    if (previous !== preset && !('Active_preset' in portValues)) { answer('Preset_combobox', preset - 1); }
    if (previous !== preset && 'Active_preset' in portValues) { send({ type: 'change', symbol: 'Active_preset', value: preset - 1 }); }
  }

  // A factory preset of the plugin, loaded the way MOD UI does: every port it lists, then the parameters (the names of the scenes)
  function loadPreset(index) {
    var preset = CFG.presets[index];
    if (!preset) { return; }
    Object.keys(preset.ports).forEach(function (symbol) {
      if (!(symbol in portValues)) { return; }
      send({ type: 'change', symbol: symbol, value: preset.ports[symbol] });
    });
    Object.keys(preset.state).forEach(function (uri) {
      parameterValues[uri] = preset.state[uri];
      send({ type: 'patch_set', uri: uri, value: preset.state[uri] });
    });
    if (simulate) {
      for (var i = 1; i < 64 && 'Preset_selector_' + i in portValues; i++) { if (portValues['Preset_selector_' + i] === 1) { hostPreset = i; } }
    }
  }

  // ------------------------------------------------------------------------------------------ voltage meters
  // The manual (not MOD UI) writes the voltage of each CV output beside its jack, so it is clear when it is 0 V and when 10 V.
  // spec (from interactive.json): { select: the jacks, on: "Preset_{active}_output_{n}", volts: 10 }. {n} is the place of the jack
  // among the ones selected (1, 2...), {active} the preset the simulated host plays; the output is on when that port is 1
  var metersSpec = null;
  function activePreset() {
    for (var i = 1; 'Preset_selector_' + i in portValues; i++) { if (portValues['Preset_selector_' + i] == 1) { return i; } }
    return 1;
  }
  function updateMeters() {
    if (!metersSpec) { return; }
    [].forEach.call(doc.querySelectorAll(metersSpec.select), function (jack, index) {
      var symbol = metersSpec.on.replace('{active}', activePreset()).replace('{n}', index + 1);
      var on = portValues[symbol] == 1;
      var chip = jack.querySelector('.ix-volt');
      if (!chip) { chip = doc.createElement('span'); chip.className = 'ix-volt'; chip.setAttribute('role', 'img'); jack.appendChild(chip); }
      chip.textContent = (on ? metersSpec.volts : 0) + ' V';
      chip.classList.toggle('is-on', on);
      chip.setAttribute('aria-label', 'Output ' + (index + 1) + ': ' + (on ? metersSpec.volts : 0) + ' volts');
    });
  }
  Host.setMeters = function (spec) { metersSpec = spec; updateMeters(); measure(); fit(); };

  // ------------------------------------------------------------------------------------------ views
  function setView(name, quiet) {
    if (name === 'settings' && !settings.children().length) { return; }
    view = name;
    win.prop('hidden', name !== 'settings');
    if (name === 'settings') { try { window.dispatchEvent(new Event('resize')); } catch (e) { /* old browser */ } }
    refresh();
    if (!quiet && Host.handlers.view) { Host.handlers.view(name); }
  }
  Host.view = setView;
  Host.currentView = function () { return view; };

  // The gear above the icon, and the Close button of the settings page: what MOD UI does with them
  $(doc).on('click', '.mod-actions .mod-settings', function () { setView('settings'); });
  $(doc).on('keydown', '.mod-actions .mod-settings', function (e) { if (e.key === 'Enter' || e.key === ' ') { setView('settings'); e.preventDefault(); } });
  $(doc).on('click', '.js-close', function () { setView('pedalboard'); return false; });

  // Fits the pedalboard view into the frame: the icon with its jacks and the gear, as large as the frame allows
  var natural = null;
  var pad = function () { return innerWidth < 500 ? 6 : 18; };
  function measure() {
    stage.css('transform', 'none');
    var el = icon[0];
    var base = el.getBoundingClientRect();
    var box = { left: 0, top: 0, right: base.width, bottom: base.height };
    icon.find('.mod-pedal-input, .mod-pedal-output, .mod-actions, .ix-volt').each(function () {
      var r = this.getBoundingClientRect();
      box.left = Math.min(box.left, r.left - base.left);
      box.top = Math.min(box.top, r.top - base.top);
      box.right = Math.max(box.right, r.right - base.left);
      box.bottom = Math.max(box.bottom, r.bottom - base.top);
    });
    natural = box;
    Host.natural = { width: box.right - box.left, height: box.bottom - box.top };
  }
  function fit() {
    if (!natural) { measure(); }
    var width = natural.right - natural.left, height = natural.bottom - natural.top;
    var scale = Math.min(1, (innerWidth - 2 * pad()) / width, (innerHeight - 2 * pad()) / height);
    Host.scale = scale;
    stage.css('transform', 'scale(' + scale + ') translate(' + (-(natural.left + width / 2)) + 'px,' + (-(natural.top + height / 2)) + 'px)');
    refresh();
  }
  window.addEventListener('resize', function () { fit(); });

  // ------------------------------------------------------------------------------------------ what is on the screen
  var overlay = $('<div class="ix-overlay" aria-hidden="true">').appendTo(doc.body);
  var ringEl = $('<div class="ix-ring">').appendTo(overlay);
  var fingerEl = $('<div class="ix-finger">').appendTo(overlay);
  var spotsEl = $('<div>').appendTo(overlay);
  var ringTarget = null;
  var fingerAt = null;

  var visible = function (el) {
    if (!el || !el.getClientRects().length) { return false; }
    var style = getComputedStyle(el);
    return style.visibility !== 'hidden' && style.display !== 'none';
  };
  // The view that is showing: what the settings window covers is not on the screen
  var inView = function (el) {
    return visible(el) && (view === 'settings' ? win[0].contains(el) || dialog[0].contains(el) : !win[0].contains(el));
  };
  // `has`: only the elements whose text holds it (a factory preset is found by its name)
  var find = function (selector, has) {
    var all = [].slice.call(doc.querySelectorAll(selector));
    return all.filter(function (el) { return inView(el) && (!has || (el.textContent || '').indexOf(has) >= 0); })[0] || null;
  };
  var resolve = function (target) {
    if (typeof target === 'string') { return find(target); }
    if (target && target.nodeType) { return target; }
    return target && target.select ? find(target.select, target.has) : null;
  };

  // The pages of the settings page scroll sideways: bring what is on another page into view, as a swipe would
  function reveal(el, instant) {
    for (var node = el.parentElement; node && node !== doc.body; node = node.parentElement) {
      var overflow = getComputedStyle(node).overflowX;
      if ((overflow === 'auto' || overflow === 'scroll') && node.scrollWidth > node.clientWidth + 1) {
        var r = el.getBoundingClientRect(), n = node.getBoundingClientRect();
        if (r.left < n.left - 1 || r.right > n.right + 1) {
          var left = node.scrollLeft + (r.left - n.left) - Math.max(0, (n.width - r.width) / 2);
          node.scrollTo({ left: left, behavior: instant || reduced ? 'auto' : 'smooth' });
          return true;
        }
      }
    }
    return false;
  }
  Host.reveal = reveal;

  function place(node, el, pad) {
    var r = el.getBoundingClientRect();
    node.css({ left: r.left - pad, top: r.top - pad, width: r.width + 2 * pad, height: r.height + 2 * pad });
  }

  var last = '';
  function refresh() {
    // A ring and the outlines follow their element (the pager scrolls, the frame is resized, the view changes)
    var key = [view, innerWidth, innerHeight];
    if (ringTarget && inView(ringTarget)) { place(ringEl, ringTarget, 4); key.push(ringEl.css('left')); ringEl.addClass('is-on'); } else { ringEl.removeClass('is-on'); }
    if (Host.showing) { drawSpots(); }
    return key.join();
  }

  var ticking = false;
  // What the ring and the outlines depend on: where their elements are (the pager scrolls, the frame is resized, a name grows...)
  function signature() {
    var parts = [view, innerWidth, innerHeight];
    if (ringTarget && inView(ringTarget)) { var r = ringTarget.getBoundingClientRect(); parts.push(r.left, r.top, r.width, r.height); }
    if (Host.showing) {
      Host.areas.forEach(function (area) {
        if (area.view && area.view !== view) { return; }
        [].forEach.call(doc.querySelectorAll(area.select), function (el) {
          if (!inView(el)) { return; }
          var b = el.getBoundingClientRect();
          parts.push(Math.round(b.left), Math.round(b.top), Math.round(b.width), Math.round(b.height));
        });
      });
    }
    return parts.join();
  }
  function loop() {
    // Only while there is something to follow
    if (!ringTarget && !Host.showing) { ticking = false; return; }
    var now = signature();
    if (now !== last) { last = now; refresh(); }
    requestAnimationFrame(loop);
  }
  function follow() { if (!ticking) { ticking = true; requestAnimationFrame(loop); } }

  Host.ring = function (target) {
    var el = target ? resolve(target) : null;
    ringTarget = el;
    if (el) { reveal(el); }
    refresh();
    if (el) { follow(); }
    return !!el;
  };

  // "Show areas": a dashed outline on everything that can be tapped, and a numbered pin on the first of each kind
  function drawSpots() {
    spotsEl.empty();
    var shown = [];
    Host.areas.forEach(function (area) {
      if (area.view && area.view !== view) { return; }
      shown.push({ area: area, matches: [].slice.call(doc.querySelectorAll(area.select)).filter(inView) });
    });
    shown.forEach(function (entry, number) {
      entry.matches.forEach(function (el, i) {
        var r = el.getBoundingClientRect();
        if (r.right < 0 || r.left > innerWidth || r.bottom < 0 || r.top > innerHeight) { return; }
        $('<div class="ix-spot">').css({ left: r.left - 3, top: r.top - 3, width: r.width + 6, height: r.height + 6 }).appendTo(spotsEl);
        if (i === 0) {
          // A part that holds other parts (a bank holds its switches) has its pin at the other corner, so they do not pile up
          var holds = shown.some(function (other) { return other !== entry && other.matches.some(function (m) { return m !== el && el.contains(m); }); });
          $('<div class="ix-pin">').text(entry.area.n || number + 1).css({ left: holds ? r.right : r.left, top: r.top }).appendTo(spotsEl);
        }
      });
    });
  }
  Host.showAreas = function (on) { Host.showing = !!on; spotsEl.empty(); if (on) { drawSpots(); follow(); } };
  Host.setAreas = function (areas) { Host.areas = areas; if (Host.showing) { drawSpots(); } };

  // ------------------------------------------------------------------------------------------ what a tap on a part says
  var synthetic = false;
  function hit(target) {
    for (var i = 0; i < Host.areas.length; i++) {
      var area = Host.areas[i];
      if (area.view && area.view !== view) { continue; }
      var el = target.closest ? target.closest(area.select) : null;
      if (el) {
        var data = Object.assign({}, el.dataset);
        data.title = el.getAttribute('title') || el.getAttribute('aria-label') || '';
        data.symbol = el.getAttribute('mod-port-symbol') || '';
        return { index: i, id: area.id, el: el, data: data };
      }
    }
    return null;
  }
  doc.addEventListener('click', function (event) {
    if (synthetic || !Host.handlers.select || !event.isTrusted) { return; }
    var found = hit(event.target);
    if (found) { Host.handlers.select(found); }
  }, true);

  // ------------------------------------------------------------------------------------------ the finger
  var overlayBox = function () { return overlay[0].getBoundingClientRect(); };
  var fingerPos = function (x, y) { fingerEl[0].style.setProperty('--x', x + 'px'); fingerEl[0].style.setProperty('--y', y + 'px'); };
  var centre = function (el) {
    var r = el.getBoundingClientRect();
    return { x: r.left + r.width * 0.55, y: r.top + r.height * 0.55 };
  };

  // Where the finger rests when it has nothing to do (where it was last, or the middle of the view)
  async function glideTo(el, instant) {
    var p = centre(el);
    if (!fingerAt) {
      // First time: it appears a little above the target, without gliding from the corner
      fingerEl[0].style.setProperty('--glide', '0s');
      fingerPos(p.x, p.y - 60);
      void fingerEl[0].offsetWidth;
    }
    fingerEl[0].style.setProperty('--glide', (instant || reduced ? 0 : 0.7 / Host.speed) + 's');
    fingerEl.addClass('is-on');
    fingerPos(p.x, p.y);
    fingerAt = p;
    if (!instant) { await sleep((reduced ? 120 : 760) / Host.speed); }
  }
  Host.hideFinger = function () { fingerEl.removeClass('is-on is-down'); fingerAt = null; };

  function mouse(el, type) {
    var p = centre(el);
    var init = { bubbles: true, cancelable: true, view: window, clientX: p.x, clientY: p.y, button: 0 };
    if (type.indexOf('pointer') === 0 && typeof PointerEvent === 'function') { el.dispatchEvent(new PointerEvent(type, Object.assign({ pointerType: 'touch', isPrimary: true }, init))); return; }
    el.dispatchEvent(new MouseEvent(type, init));
  }

  // Taps the first element that matches: the finger glides there, presses and lets go, and the page gets the clicks a touch would send
  Host.tap = async function (target, options) {
    var instant = !!(options && options.instant);
    var el = resolve(target);
    if (!el) { Host.log.push(['tap: nothing at', String(target)]); return false; }
    if (reveal(el, instant)) { await sleep(instant ? 0 : 650 / Host.speed); }
    if (!instant) {
      await glideTo(el);
      fingerEl.addClass('is-down');
      el.classList.add('ix-pressed');
      await sleep(120 / Host.speed);
      var p = centre(el);
      var ripple = doc.createElement('div');
      ripple.className = 'ix-ripple';
      ripple.style.setProperty('--x', p.x + 'px');   // (jQuery 1.9 does not know custom properties)
      ripple.style.setProperty('--y', p.y + 'px');
      overlay[0].appendChild(ripple);
      ripple.addEventListener('animationend', function () { ripple.remove(); });
    }
    synthetic = true;
    try {
      ['pointerdown', 'mousedown'].forEach(function (type) { mouse(el, type); });
      if (el.focus && /^(input|textarea|select)$/i.test(el.tagName)) { el.focus({ preventScroll: true }); }
      ['pointerup', 'mouseup'].forEach(function (type) { mouse(el, type); });
      el.click();
    } finally { synthetic = false; }
    if (!instant) {
      el.classList.remove('ix-pressed');
      fingerEl.removeClass('is-down');
      await sleep(160 / Host.speed);
    }
    return true;
  };

  // Types into a text field, one letter at a time (the field is tapped first, and its text selected)
  Host.type = async function (target, text, options) {
    var instant = !!(options && options.instant);
    var el = resolve(target);
    if (!el) { Host.log.push(['type: nothing at', String(target)]); return false; }
    await Host.tap(el, options);
    var set = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
    var write = function (value) { set.call(el, value); el.dispatchEvent(new Event('input', { bubbles: true })); };
    synthetic = true;
    try {
      el.focus({ preventScroll: true });
      if (instant) { write(text); } else { el.select(); await sleep(250 / Host.speed); }
      if (!instant) {
        for (var i = 0; i < text.length; i++) {
          write(text.slice(0, i + 1));
          await sleep(120 / Host.speed);
        }
        await sleep(300 / Host.speed);
      }
      el.blur();
    } finally { synthetic = false; }
    return true;
  };

  // ------------------------------------------------------------------------------------------ MOD UI's own dialogs (a simplified drawing of them)
  var dialog = $('<div class="ix-dialog-shade" hidden><div class="ix-dialog" role="dialog" aria-modal="true" aria-labelledby="ix-dialog-title">' +
    '<b id="ix-dialog-title">Assign to a control</b><small>MOD UI · simplified in this manual</small>' +
    '<p></p><div class="ix-fake" aria-hidden="true"><span class="is-pick">Footswitch A</span><span>Footswitch B</span><span>Knob 1</span><span>MIDI learn</span></div>' +
    '<div class="ix-buttons"><button type="button" class="ix-cancel">Cancel</button><button type="button" class="ix-ok">Assign</button></div></div></div>').appendTo(doc.body);
  var pending = null;
  $(doc).on('click', '.mod-address:not(:disabled)', function () {
    if ($(this).closest('.ix-dialog-shade').length) { return; }
    pending = this;
    var name = this.getAttribute('title') || 'this control';
    dialog.find('p').text('MOD UI opens its own dialog here. ' + name + '. Pick the control of your device (a footswitch, a knob, a MIDI controller) and confirm.');
    dialog.find('.ix-ok').text($(this).hasClass('addressed') ? 'Remove' : 'Assign');
    dialog.prop('hidden', false);
  });
  dialog.on('click', '.ix-ok', function () { if (pending) { $(pending).toggleClass('addressed'); } dialog.prop('hidden', true); });
  dialog.on('click', '.ix-cancel', function () { dialog.prop('hidden', true); });
  dialog.on('click', function (e) { if (e.target === dialog[0]) { dialog.prop('hidden', true); } });

  // The factory presets of the plugin: choosing one loads it (and says it is the one chosen)
  $(doc).on('click', '.mod-preset-factory [mod-role="enumeration-option"]', function () {
    $(this).siblings().removeClass('selected');
    $(this).addClass('selected');
    loadPreset(Number(String($(this).attr('mod-uri')).replace('preset', '')));
  });
  $(doc).on('click', '.mod-preset-user [mod-role="enumeration-option"]', function () { $(this).siblings().removeClass('selected'); $(this).addClass('selected'); });
  $(doc).on('change', '.mod-presets .radio-preset', function () {
    var user = this.value === 'user';
    $(this).closest('.mod-presets').find('.mod-preset-factory').toggleClass('mod-hidden', user).end().find('.mod-preset-user').toggleClass('mod-hidden', !user);
  });
  $('<style>.mod-hidden { display: none !important; }</style>').appendTo(doc.head);

  Host.hasDialog = function () { return !dialog.prop('hidden'); };

  // ------------------------------------------------------------------------------------------ start
  var ports = [{ symbol: ':bypass', value: 0 }].concat(CFG.ports.map(function (p) { return { symbol: p.symbol, value: portValues[p.symbol] }; }));
  send({ type: 'start', ports: ports, parameters: Object.keys(parameterValues).map(function (uri) { return { uri: uri, value: parameterValues[uri] }; }) });
  measure();
  fit();
  Host.ready = true;
  Host.version = 1;
  if (window.parent !== window && window.parent.__ixReady) { window.parent.__ixReady(Host); }
})();
