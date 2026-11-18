/* Copyright 2021-2026 Paulo Mateus -- SPDX-License-Identifier: Apache-2.0

   The interactive part of a manual (docs/interactive-manuals.md): the page around the frame of the plugin. The plugin itself runs in
   the frame (host.js); this file builds what is beside it:
     * Explore: tap a part of the plugin and read what it is, or pick it from the list of parts;
     * Guided tours: a list of common tasks. A tour is taken one step at a time (Next, Back), and a finger shows
       where to tap and the description of the step says what is happening.
   The page carries its data as JSON (areas and tours) and the document of the frame as a JSON string (see interactive.py). */
(function () {
  'use strict';

  var reduced = !!(window.matchMedia && window.matchMedia('(prefers-reduced-motion: reduce)').matches);
  var sleep = function (ms) { return new Promise(function (resolve) { setTimeout(resolve, ms); }); };
  var el = function (tag, className, html) {
    var node = document.createElement(tag);
    if (className) { node.className = className; }
    if (html !== undefined) { node.innerHTML = html; }
    return node;
  };
  var esc = function (text) { return String(text).replace(/[&<>"]/g, function (c) { return { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]; }); };
  var fill = function (text, data) { return text.replace(/\{(\w+)\}/g, function (m, key) { return data && data[key] !== undefined ? esc(data[key]) : ''; }); };
  var plain = function (html) { var d = document.createElement('div'); d.innerHTML = html; return d.textContent || ''; };

  document.querySelectorAll('.ix[data-ix]').forEach(function (root) {
    try { init(root); } catch (error) { root.insertAdjacentHTML('beforeend', '<p class="ix-end">The interactive manual could not start in this browser.</p>'); if (window.console) { console.error(error); } }
  });

  function init(root) {
    var cfg = JSON.parse(root.querySelector('script[data-ix-config]').textContent);
    var frameDoc = JSON.parse(root.querySelector('script[data-ix-doc]').textContent);
    var T = cfg.text;
    var views = cfg.views;
    var hasSettings = views.length > 1;

    // ---------------------------------------------------------------- the skeleton
    var seg = function (label, buttons) {
      var group = el('div', 'ix-seg'); group.setAttribute('role', 'group'); group.setAttribute('aria-label', label);
      buttons.forEach(function (b) { group.appendChild(b); });
      return group;
    };
    var button = function (className, html, attrs) {
      var b = el('button', className, html); b.type = 'button';
      Object.keys(attrs || {}).forEach(function (k) { b.setAttribute(k, attrs[k]); });
      return b;
    };

    var modeButtons = { explore: button('', T.explore, { 'aria-pressed': 'true' }), tour: button('', T.tours, { 'aria-pressed': 'false' }) };
    var viewButtons = {};
    views.forEach(function (v) { viewButtons[v.id] = button('', esc(v.label), { 'aria-pressed': 'false' }); });
    var areasButton = button('ix-btn', T.showAreas, { 'aria-pressed': 'false' });
    var resetButton = button('ix-btn', T.reset);

    var top = el('div', 'ix-top');
    top.appendChild(seg(T.mode, [modeButtons.explore, modeButtons.tour]));
    var tools = el('div', 'ix-tools');
    if (hasSettings) { tools.appendChild(seg(T.view, views.map(function (v) { return viewButtons[v.id]; }))); }
    tools.appendChild(areasButton);
    tools.appendChild(resetButton);
    top.appendChild(tools);

    var stageBox = el('div', 'ix-stage');
    var frame = el('iframe', 'ix-frame is-loading');
    frame.title = cfg.frameTitle;
    frame.setAttribute('loading', 'lazy');
    var shield = el('div', 'ix-shield'); shield.hidden = true;
    var loading = el('div', 'ix-loading', T.loading);
    stageBox.appendChild(frame); stageBox.appendChild(shield); stageBox.appendChild(loading);

    var hint = el('p', 'ix-hint', esc(T.phoneHint)); hint.hidden = true;
    var explore = el('div', 'ix-explore');
    var card = el('div', 'ix-card is-empty'); card.setAttribute('aria-live', 'polite');
    var chipsTitle = el('p', 'ix-chips-title', T.parts);
    var chips = el('ul', 'ix-chips');
    explore.appendChild(card); explore.appendChild(chipsTitle); explore.appendChild(chips);

    var picker = el('div', 'ix-picker'); picker.hidden = true;
    var tourList = el('ul', 'ix-tours');
    picker.appendChild(el('p', 'ix-chips-title', T.chooseTour));
    picker.appendChild(tourList);

    var player = el('div', 'ix-player'); player.hidden = true;
    var playerTitle = el('h3');
    var backToList = button('ix-btn', T.allTours);
    var head = el('div', 'ix-player-head'); head.appendChild(playerTitle); head.appendChild(backToList);
    var bar = el('div', 'ix-bar'); var barFill = el('i'); bar.appendChild(barFill);
    var prevButton = button('ix-btn', '<span aria-hidden="true">&#9664;</span> ' + T.back);
    var nextButton = button('ix-btn', T.next + ' <span aria-hidden="true">&#9654;</span>');
    var count = el('span', 'ix-count');
    var controls = el('div', 'ix-controls');
    [prevButton, nextButton, count].forEach(function (n) { controls.appendChild(n); });
    var say = el('div', 'ix-say'); say.setAttribute('aria-live', 'polite');
    var stepList = el('ul', 'ix-steps');
    var end = el('p', 'ix-end'); end.hidden = true;
    [head, bar, controls, say, stepList, end].forEach(function (n) { player.appendChild(n); });

    var printNote = el('p', 'ix-print', '<em>' + esc(T.print) + '</em>');
    root.textContent = '';
    root.appendChild(top); root.appendChild(stageBox); root.appendChild(hint); root.appendChild(explore); root.appendChild(picker); root.appendChild(player); root.appendChild(printNote);
    root.removeAttribute('data-ix-loading');

    // ---------------------------------------------------------------- the frame
    var host = null;
    var view = views[0].id;
    var mode = 'explore';
    var started = false;

    function load() {
      started = true;
      frame.classList.add('is-loading');
      loading.hidden = false;
      host = null;
      return new Promise(function (resolve) {
        frame.onload = function () {
          try { host = frame.contentWindow.Host; } catch (error) { host = null; }
          if (!host || !host.ready) { loading.textContent = T.failed; resolve(null); return; }
          loading.hidden = true;
          host.setAreas(areasOf(view));
          host.handlers.select = onSelect;
          host.handlers.view = function (name) { view = name; syncView(); };
          if (cfg.meters && host.setMeters) { host.setMeters(cfg.meters); }
          if (cfg.midi && host.setMidi) { host.setMidi(cfg.midi); }
          host.showAreas(areasButton.getAttribute('aria-pressed') === 'true');
          host.view(view, true);
          syncView();
          frame.classList.remove('is-loading');
          resolve(host);
        };
        frame.srcdoc = frameDoc;
      });
    }

    // The frame is loaded when it comes near the screen: a manual is long and most readers never get to this section
    if ('IntersectionObserver' in window) {
      var watcher = new IntersectionObserver(function (entries) {
        if (entries.some(function (e) { return e.isIntersecting; })) { watcher.disconnect(); if (!started) { load(); } }
      }, { rootMargin: '300px 0px' });
      watcher.observe(root);
    } else { load(); }

    function frameHeight() {
      var width = frame.clientWidth || stageBox.clientWidth;
      if (view === 'settings') { return width < 520 ? 580 : Math.max(440, Math.min(560, Math.round(width * 0.62))); }
      var nat = host && host.natural;
      if (!nat) { return 360; }
      var gap = width < 500 ? 12 : 36;
      return Math.round(nat.height * Math.min(1, (width - gap) / nat.width) + gap);
    }
    function resize() { frame.style.height = frameHeight() + 'px'; hint.hidden = !(hasSettings && view === 'pedalboard' && stageBox.clientWidth < 560); }
    if ('ResizeObserver' in window) { new ResizeObserver(resize).observe(stageBox); } else { window.addEventListener('resize', resize); }

    function areasOf(name) {
      var n = 0;
      return cfg.areas.filter(function (a) { return !a.view || a.view === name; }).map(function (a) { a.n = ++n; return a; });
    }

    function syncView() {
      views.forEach(function (v) { viewButtons[v.id].setAttribute('aria-pressed', String(v.id === view)); });
      if (host) { host.setAreas(areasOf(view)); }
      resize();
      buildChips();
      if (host && host.ring) { host.ring(null); }
    }

    function setView(name) {
      if (!host || name === view) { return; }
      view = name;
      host.view(name, true);
      syncView();
    }
    views.forEach(function (v) { viewButtons[v.id].addEventListener('click', function () { setView(v.id); showIntro(); }); });

    // ---------------------------------------------------------------- explore
    var selected = null;
    function showIntro() { selected = null; card.className = 'ix-card is-empty'; card.innerHTML = '<p>' + T.intro + '</p>'; buildChips(); }

    function showArea(area, data) {
      selected = area.id;
      var links = (area.links || []).map(function (l) { return '<li><a href="' + esc(l.href) + '">' + esc(l.label) + '</a></li>'; }).join('');
      card.className = 'ix-card';
      card.innerHTML = '<p class="ix-kicker">' + esc(area.kicker || views.filter(function (v) { return v.id === view; })[0].label) + '</p>' +
        '<h3>' + fill(area.title, data) + '</h3>' + fill(area.text, data) + (links ? '<ul>' + links + '</ul>' : '');
      buildChips();
    }

    function onSelect(found) {
      var area = cfg.areas.filter(function (a) { return a.id === found.id; })[0];
      if (!area) { return; }
      showArea(area, found.data);
      host.ring(found.el);
    }

    function buildChips() {
      chips.textContent = '';
      areasOf(view).forEach(function (area) {
        var li = el('li');
        var b = button('ix-chip', '<b>' + area.n + '</b>' + esc(area.chip || plain(area.title)), { 'aria-pressed': String(selected === area.id) });
        b.addEventListener('click', function () {
          if (!host) { return; }
          var target = host.ring(area.select);
          // What the part is called in the card: the first one of its kind
          var first = frame.contentDocument.querySelector(area.select);
          showArea(area, first ? Object.assign({}, first.dataset, { title: first.getAttribute('title') || '' }) : {});
          if (!target) { host.ring(null); }
        });
        li.appendChild(b);
        chips.appendChild(li);
      });
    }

    areasButton.addEventListener('click', function () {
      var on = areasButton.getAttribute('aria-pressed') !== 'true';
      areasButton.setAttribute('aria-pressed', String(on));
      if (host) { host.showAreas(on); }
    });

    resetButton.addEventListener('click', function () {
      stopTour(true);
      load().then(function () { if (mode === 'explore') { showIntro(); } else { showTourIntro(); } });
    });

    // ---------------------------------------------------------------- modes
    function setMode(next) {
      if (mode === next) { return; }
      mode = next;
      Object.keys(modeButtons).forEach(function (m) { modeButtons[m].setAttribute('aria-pressed', String(m === mode)); });
      explore.hidden = mode !== 'explore';
      picker.hidden = mode !== 'tour' || !!tour;
      player.hidden = mode !== 'tour' || !tour;
      if (mode === 'explore') { stopTour(false); shield.hidden = true; setTools(true); showIntro(); }
      else { if (host) { host.ring(null); } }
    }
    modeButtons.explore.addEventListener('click', function () { setMode('explore'); });
    modeButtons.tour.addEventListener('click', function () { setMode('tour'); });

    function setTools(on) {
      [areasButton, resetButton].concat(views.map(function (v) { return viewButtons[v.id]; })).forEach(function (b) { b.disabled = !on; });
      if (!on && host) { host.showAreas(false); areasButton.setAttribute('aria-pressed', 'false'); }
    }

    // ---------------------------------------------------------------- tours
    var tour = null;          // the tour being shown
    var at = -1;              // the step shown
    var token = 0;            // changes whenever the tour is stopped or moved: what waits for it gives up
    var busy = false;

    cfg.tours.forEach(function (t, i) {
      var li = el('li');
      var b = button('ix-tour', '<b>' + esc(t.title) + '</b><span>' + esc(t.summary || '') + '</span><small>' + t.steps.length + ' ' + T.steps + '</small>');
      b.addEventListener('click', function () { openTour(i); });
      li.appendChild(b);
      tourList.appendChild(li);
    });
    if (!cfg.tours.length) { modeButtons.tour.hidden = true; }

    function showTourIntro() { tour = null; at = -1; picker.hidden = false; player.hidden = true; shield.hidden = true; setTools(true); }

    function stopTour(keepView) {
      token++; busy = false;
      if (host && host.hideFinger) { host.hideFinger(); }
      if (!keepView) { tour = null; at = -1; }
      if (keepView) { tour = null; at = -1; picker.hidden = mode !== 'tour'; player.hidden = true; }
    }

    async function openTour(i) {
      tour = cfg.tours[i];
      picker.hidden = true; player.hidden = false;
      playerTitle.textContent = tour.title;
      stepList.textContent = '';
      tour.steps.forEach(function (s, k) {
        var li = el('li');
        var b = button('', String(k + 1), { 'aria-label': T.step + ' ' + (k + 1) + ': ' + (s.title || '') });
        b.addEventListener('click', function () { goTo(k, false); });
        li.appendChild(b); stepList.appendChild(li);
      });
      shield.hidden = false;
      setTools(false);
      await goTo(0, true, true);
    }

    // The part a step is about: what it says to ring, or else what its first action touches
    function ringOf(step) {
      if (step.ring === false) { return null; }
      if (step.ring) { return step.ring; }
      var first = step.do && step.do[0];
      return first && (first.tap || first.type) ? (first.has ? { select: first.tap || first.type, has: first.has } : first.tap || first.type) : null;
    }

    // What the steps do, in the frame: show the view, ring the part, and tap or type with the finger
    async function runStep(step, instant, mine) {
      if (!host) { return; }
      if (step.view && step.view !== view) { view = step.view; host.view(step.view, true); syncView(); if (!instant) { await sleep(450); } }
      if (mine !== token) { return; }
      var ringed = ringOf(step);
      host.ring(instant ? null : ringed);
      if (!instant && ringed) { await sleep(reduced ? 0 : 350); }
      for (var i = 0; i < (step.do || []).length; i++) {
        if (mine !== token) { return; }
        var action = step.do[i];
        var target = action.has ? { select: action.tap || action.type, has: action.has } : (action.tap || action.type);
        if (action.tap) { await host.tap(target, { instant: instant }); }
        else if (action.type) { await host.type(target, action.text || '', { instant: instant }); }
        else if (action.wait && !instant) { await sleep(action.wait); }
        else if (action.view) { host.view(action.view, true); view = action.view; syncView(); }
      }
      if (instant) { host.ring(null); }
    }

    function show(k) {
      var step = tour.steps[k];
      at = k;
      say.innerHTML = '<h4>' + esc(step.title || '') + '</h4>' + step.text;
      count.textContent = T.step + ' ' + (k + 1) + ' ' + T.of + ' ' + tour.steps.length;
      barFill.style.width = ((k + 1) / tour.steps.length * 100) + '%';
      [].slice.call(stepList.querySelectorAll('button')).forEach(function (b, n) {
        b.classList.toggle('is-done', n < k);
        if (n === k) { b.setAttribute('aria-current', 'step'); } else { b.removeAttribute('aria-current'); }
      });
      prevButton.disabled = k === 0;
      nextButton.disabled = k === tour.steps.length - 1;
      end.hidden = k !== tour.steps.length - 1;
    }

    // To step k. Going forward by one is shown with the finger; anything else starts from the clean plugin and does what the
    // steps before it did at once (and the step itself, when `finger` is false), so the frame always agrees with the text
    async function goTo(k, finger, fresh) {
      var mine = ++token;
      busy = true;
      var forward = finger && at === k - 1 && !fresh;
      if (!forward) {
        if (host) { host.hideFinger(); }
        await load();
        if (mine !== token) { return; }
        for (var i = 0; i < k + (finger ? 0 : 1); i++) { await runStep(tour.steps[i], true, mine); if (mine !== token) { return; } }
      }
      show(k);
      if (finger) { await runStep(tour.steps[k], false, mine); }
      else { host.ring(ringOf(tour.steps[k])); }
      if (mine === token) { busy = false; }
    }

    // The end of a tour: say so, and offer to go again or to try it
    var again = button('ix-btn', T.replay);
    var own = button('ix-btn', T.tryIt);
    end.appendChild(document.createTextNode(T.finished + ' ')); end.appendChild(again); end.appendChild(document.createTextNode(' ')); end.appendChild(own);
    again.addEventListener('click', function () { goTo(0, true, true); });
    own.addEventListener('click', function () { setMode('explore'); });

    nextButton.addEventListener('click', function () { if (at < tour.steps.length - 1) { goTo(at + 1, true); } });
    prevButton.addEventListener('click', function () { if (at > 0) { goTo(at - 1, false); } });
    backToList.addEventListener('click', function () {
      stopTour(true); shield.hidden = true; setTools(true);
      if (host) { host.ring(null); }
    });

    showIntro();
    resize();
  }
})();
