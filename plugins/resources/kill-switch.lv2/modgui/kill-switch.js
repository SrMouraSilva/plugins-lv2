function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.

  // Mirrors PATTERNS in plugins/src/kill-switch.c (and the scale points of `pattern` in kill-switch.ttl): '0' = cut,
  // '1' = half, '2' = the signal passes. The last pattern, Custom, is the one written on the settings page: its steps
  // are the ports custom_step_N. Keep them in step
  const PATTERNS = [
    { name: 'Chop 8ths',           steps: '02',               perBeat: 2 },
    { name: 'Chop 16ths',          steps: '02',               perBeat: 4 },
    { name: 'Triplet chop',        steps: '02',               perBeat: 3 },
    { name: 'Machine gun',         steps: '02',               perBeat: 8 },
    { name: 'Syncopated 3-3-2',    steps: '02202202',         perBeat: 2 },
    { name: 'Gallop',              steps: '0220',             perBeat: 4 },
    { name: 'Morello solo chop',   steps: '0202022002022022', perBeat: 4 },
    { name: 'Morello toggle solo', steps: '0222020200220202', perBeat: 4 },
    { name: 'Greenwood chunk',     steps: '02220222',         perBeat: 2 },
    { name: 'Buckethead burst',    steps: '0202020222222222', perBeat: 8 },
    { name: 'Custom',              steps: null,               perBeat: null },
  ];
  const CUSTOM = PATTERNS.length - 1;          // the value of `pattern` that plays the custom pattern
  const CUSTOM_STEPS = 16;
  const CUSTOM_MAX_PER_BEAT = 8;
  const LEVELS = 3;
  const LEVEL_NAMES = ['Cut', 'Half', 'Pass'];
  const BEAT_NAMES = { 1: 'quarters', 2: '8ths', 3: 'triplets', 4: '16ths', 6: 'sextuplets', 8: '32nds' };
  const SPEEDS = 4;
  const BPM = { min: 40, max: 240 };
  const EDGE = { min: 0.1, max: 20 };          // ms, a logarithmic port: the slider is 0..EDGE_SLIDER
  const EDGE_SLIDER = 1000;
  const DISPLAYED_STEPS = 16;
  const SEND_EVERY_MS = 40;                    // a slider that is being dragged sends at most this often
  const HOLD_DELAY_MS = 400;                   // holding - or + repeats after this, every HOLD_EVERY_MS
  const HOLD_EVERY_MS = 70;

  const stepSymbol = (index) => `custom_step_${index + 1}`;
  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const now = () => Date.now();
  const range = (from, to) => Array.from({ length: to - from + 1 }, (_, i) => from + i);
  const px = (value) => parseFloat(value) || 0;
  const element = (className) => {
    const el = document.createElement('div');
    el.className = className;
    return el;
  };

  const edgeToSlider = (ms) => Math.round(EDGE_SLIDER * Math.log(clamp(ms, EDGE.min, EDGE.max) / EDGE.min) / Math.log(EDGE.max / EDGE.min));
  const sliderToEdge = (position) => EDGE.min * Math.pow(EDGE.max / EDGE.min, clamp(position, 0, EDGE_SLIDER) / EDGE_SLIDER);
  const edgeText = (ms) => `${ms < 10 ? ms.toFixed(1) : Math.round(ms)} ms`;
  const beatText = (perBeat) => BEAT_NAMES[perBeat] || `${perBeat} per beat`;

  // ---------------------------------------------------------------------------
  // State: { pattern: 0..CUSTOM, bpm, speed: 0..3, depth: 0..100, edge: ms, bypassed,
  //          custom: { length: 1..16, perBeat: 1..8, steps: [16] => 0..2 }, lastSent: [symbol] => ms, timers }

  const newState = () => ({
    pattern: 0, bpm: 120, speed: 1, depth: 100, edge: 1.5, bypassed: true,
    // The defaults of the ports in kill-switch.ttl
    custom: { length: 8, perBeat: 2, steps: [0, 2, 1, 2, 0, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2] },
    lastSent: {}, timers: {},
  });

  // The steps of a pattern as a string of levels: those of a built-in one, or the first `length` of the custom one
  const stepsOf = (state, index) => (index === CUSTOM ? state.custom.steps.slice(0, state.custom.length).join('') : PATTERNS[index].steps);

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  // (MOD UI does not send our own changes back as 'change' events, so what is drawn is what we last sent, until
  // the host says otherwise: a controller assigned to the port, a preset)
  const send = (event, symbol, value) => {
    event.data.state.lastSent[symbol] = now();
    funcs.set_port_value(symbol, value);
  };

  // While a slider is dragged: at most one message every SEND_EVERY_MS, and the last value always gets there
  const sendThrottled = (event, symbol, value) => {
    const state = event.data.state;
    const wait = SEND_EVERY_MS - (now() - (state.lastSent[symbol] || 0));

    clearTimeout(state.timers[symbol]);
    if (wait <= 0) {
      send(event, symbol, value);
    } else {
      state.timers[symbol] = setTimeout(() => send(event, symbol, value), wait);
    }
  };

  const setPattern = (event, index) => {
    const count = PATTERNS.length;
    const pattern = (index % count + count) % count;       // the arrows go round

    event.data.state.pattern = pattern;
    send(event, 'pattern', pattern);
    draw(event);
  };

  const setBpm = (event, bpm) => {
    event.data.state.bpm = clamp(Math.round(bpm), BPM.min, BPM.max);
    send(event, 'bpm', event.data.state.bpm);
    draw(event);
  };

  const setSpeed = (event, speed) => {
    event.data.state.speed = speed;
    send(event, 'speed', speed);
    draw(event);
  };

  // Writing the custom pattern is meant to be heard: the first edit puts Pattern on Custom
  const editing = (event) => {
    if (event.data.state.pattern !== CUSTOM) {
      event.data.state.pattern = CUSTOM;
      send(event, 'pattern', CUSTOM);
    }
  };

  const cycleStep = (event, index) => {
    const custom = event.data.state.custom;
    editing(event);
    custom.steps[index] = (custom.steps[index] + 1) % LEVELS;      // cut -> half -> pass -> cut
    send(event, stepSymbol(index), custom.steps[index]);
    draw(event);
  };

  const setLength = (event, length) => {
    editing(event);
    event.data.state.custom.length = clamp(Math.round(length), 1, CUSTOM_STEPS);
    send(event, 'custom_length', event.data.state.custom.length);
    draw(event);
  };

  const setPerBeat = (event, perBeat) => {
    editing(event);
    event.data.state.custom.perBeat = clamp(Math.round(perBeat), 1, CUSTOM_MAX_PER_BEAT);
    send(event, 'custom_steps_per_beat', event.data.state.custom.perBeat);
    draw(event);
  };

  // Copies a built-in pattern into the custom one, to change it from there
  const copyPattern = (event, index) => {
    const source = PATTERNS[index];
    const custom = event.data.state.custom;
    if (!source || !source.steps) {
      return;
    }

    editing(event);
    custom.length = source.steps.length;
    custom.perBeat = source.perBeat;
    // The steps after the end keep the level of the pass, as in the defaults: they do not play until the length grows
    custom.steps = range(0, CUSTOM_STEPS - 1).map((i) => (i < custom.length ? Number(source.steps[i]) : 2));
    send(event, 'custom_length', custom.length);
    send(event, 'custom_steps_per_beat', custom.perBeat);
    custom.steps.forEach((level, i) => send(event, stepSymbol(i), level));
    draw(event);
  };

  // ---------------------------------------------------------------------------
  // The settings page never scrolls vertically: its panels (the pattern editor, the hardware tiles, the saved presets)
  // are spread over horizontal pages, as few as the screen allows, one swipe (or one of the buttons of the top bar)
  // apart. What fits is measured here instead of left to @media queries: MOD UI's page has no <meta name="viewport">
  // and its body is at least 1024px wide, so a phone lays it out in about 980px and shrinks it to the screen, and a
  // query on the viewport never sees a phone. See docs/custom-settings-pages.md

  const WIDE_PX = 1100;          // from this width, panels can sit side by side
  const MEDIUM_PX = 700;
  const SHORT_PX = 600;          // below this height, the descriptions go and everything gets tighter
  const CELL_MIN_PX = 44;        // the narrowest a step of the editor can be
  const TILE_MIN_PX = 170;       // the hardware assignment tiles
  const TILE_MAX_COLUMNS = 3;

  // The panels, the lists they hold, and what moves them: found once, on 'start'
  const findPanels = (root) => {
    const hardware = root.querySelector('[data-panel="hardware"]');
    return {
      root,
      pager: root.querySelector('.kss-pager'),
      stock: root.querySelector('.kss-stock'),
      pattern: { panel: root.querySelector('[data-panel="pattern"]'), body: root.querySelector('.kss-steps') },
      hardware: { panel: hardware, body: hardware.querySelector('.kss-assign'), items: Array.from(hardware.querySelectorAll('.kss-assign > li')), minTile: TILE_MIN_PX, columns: 1 },
      presets: { panel: root.querySelector('[data-panel="presets"]') },
      page: 0,
      key: '',
      pending: false,
    };
  };

  // MOD UI's page has no viewport meta tag, so a phone or a tablet shows it as a ~980px page shrunk to the screen.
  // This draws the page in the size of the screen instead: its box is made as many CSS pixels wide as the screen
  // has, and scaled up to cover the window. Only touch screens that really are shrunk are touched: a desktop
  // browser (zoomed in or out) is left alone
  const fitToScreen = (root) => {
    const style = root.style;
    const layoutW = Math.max(window.innerWidth, document.documentElement.scrollWidth);
    const layoutH = window.innerHeight;
    const screen = window.screen || {};
    const touch = !!(window.matchMedia && window.matchMedia('(pointer: coarse)').matches);
    // screen.width follows the orientation on Android but not on iOS: take the long side when the page is wide
    const device = layoutW > layoutH ? Math.max(screen.width, screen.height) : Math.min(screen.width, screen.height);
    const ratio = touch && device > 0 ? device / layoutW : 1;

    if (ratio > 0.3 && ratio < 0.95) {
      style.right = style.bottom = 'auto';
      style.width = `${layoutW * ratio}px`;
      style.height = `${layoutH * ratio}px`;
      style.transformOrigin = '0 0';
      style.transform = `scale(${1 / ratio})`;
    } else {
      style.right = style.bottom = style.width = style.height = style.transformOrigin = style.transform = '';
    }
  };

  // Everything back in the stock, in the order it was written, and the pages (with their copies) thrown away
  const gather = (view) => {
    view.stock.append(view.pattern.panel, view.hardware.panel, view.presets.panel);
    view.hardware.body.append(...view.hardware.items);
    view.pager.textContent = '';
    view.root.querySelectorAll('.kss-range').forEach((rangeEl) => { rangeEl.textContent = ''; });
  };

  const setPages = (view, groups) => {
    view.pager.textContent = '';
    return groups.map((nodes) => {
      const page = element('kss-page');
      page.append(...nodes);
      view.pager.append(page);
      return page;
    });
  };

  // What depends on the width of a panel (it must be on a page to be measured): the columns of the steps (16, 8 or 4,
  // so that every line is full) and of the hardware tiles (never more columns than tiles)
  const sizeAll = (view) => {
    const steps = view.pattern.body;
    const stepGap = px(getComputedStyle(steps).columnGap);
    let columns = CUSTOM_STEPS;
    while (columns > 4 && (steps.clientWidth - stepGap * (columns - 1)) / columns < CELL_MIN_PX) {
      columns /= 2;
    }
    steps.style.setProperty('--kss-cols', columns);

    const list = view.hardware.body;
    const gap = px(getComputedStyle(list).columnGap);
    view.hardware.columns = clamp(Math.floor((list.clientWidth + gap) / (view.hardware.minTile + gap)), 1, Math.min(TILE_MAX_COLUMNS, view.hardware.items.length));
    list.style.setProperty('--kss-cols', view.hardware.columns);
  };

  const fits = (page) => [page, ...page.querySelectorAll('.kss-panel, .kss-stack, .kss-row')]
    .every((el) => el.scrollHeight <= el.clientHeight + 1 && el.scrollWidth <= el.clientWidth + 1);

  // Roomy screens: the editor and the controls together, with the saved presets beside them or on a page of their own.
  // Returns the pages, or null when none of them fits
  const layoutTogether = (view, wide) => {
    const { pattern, hardware, presets } = view;
    const stack = (...nodes) => { const el = element('kss-stack'); el.append(...nodes); return el; };
    const attempts = [
      ...(wide ? [() => { const row = element('kss-row'); row.append(stack(pattern.panel, hardware.panel), presets.panel); return [[row]]; }] : []),
      () => [[stack(pattern.panel, hardware.panel)], [presets.panel]],
    ];

    for (const attempt of attempts) {
      gather(view);
      const pages = setPages(view, attempt());
      sizeAll(view);
      if (pages.every(fits)) {
        return pages;
      }
    }
    return null;
  };

  // How many lines of tiles fit on a page, measured with the panel alone on it
  const linesOf = (view, part) => {
    const [page] = setPages(view, [[part.panel]]);
    sizeAll(view);

    const style = getComputedStyle(page);
    const gap = px(getComputedStyle(part.body).rowGap);
    const room = page.clientHeight - px(style.paddingTop) - px(style.paddingBottom) - (part.panel.offsetHeight - part.body.offsetHeight);
    return Math.max(1, Math.floor((room + gap) / (part.items[0].offsetHeight + gap)));
  };

  // Splits the tiles into as many panels as it takes, the same size each; returns them (the first is the panel itself)
  const paginate = (part, capacity, perLine) => {
    const count = Math.ceil(part.items.length / capacity);
    const size = Math.ceil(Math.ceil(part.items.length / count) / perLine) * perLine;
    const panels = [part.panel];

    for (let i = 1; i < count; i++) {
      const copy = part.panel.cloneNode(false);
      const body = part.body.cloneNode(false);
      copy.append(part.panel.querySelector('.kss-panel-head').cloneNode(true), body);
      body.append(...part.items.slice(i * size, (i + 1) * size));
      panels.push(copy);
    }

    if (count > 1) {
      panels.forEach((panel, i) => { panel.querySelector('.kss-range').textContent = ` ${i + 1}/${count}`; });
    }
    return panels;
  };

  // Last resort and the way of the small screens: each panel on its own page(s)
  const layoutOneByOne = (view) => {
    const { pattern, hardware, presets } = view;
    gather(view);

    const tilePanels = paginate(hardware, linesOf(view, hardware) * hardware.columns, hardware.columns);
    const pages = setPages(view, [pattern.panel, ...tilePanels, presets.panel].map((panel) => [panel]));
    sizeAll(view);          // now that the panels are on a page, they have a width to measure
    return pages;
  };

  const goToPage = (view, index, smooth) => {
    const pager = view.pager;
    const left = clamp(index, 0, Math.max(0, pager.children.length - 1)) * pager.clientWidth;
    const still = !smooth || !window.matchMedia || window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    if (still || !pager.scrollTo) {
      pager.scrollLeft = left;
    } else {
      pager.scrollTo({ left, behavior: 'smooth' });
    }
  };

  const stepPage = (event, direction) => {
    const view = event.data.view;
    goToPage(view, view.page + direction, true);
  };

  // The dots, the buttons and the status for screen readers
  const updateNav = (event) => {
    const view = event.data.view;
    const pager = view.pager;
    const count = pager.children.length;
    const root = view.root;
    const dots = root.querySelector('.kss-dots');

    view.page = clamp(Math.round(pager.scrollLeft / (pager.clientWidth || 1)), 0, Math.max(0, count - 1));

    if (dots.children.length !== count) {
      dots.innerHTML = range(0, count - 1).map((i) => `<span class="kss-dot" data-page="${i}"></span>`).join('');
    }
    Array.from(dots.children).forEach((dot, i) => dot.classList.toggle('is-current', i === view.page));

    root.querySelector('.kss-prev').disabled = view.page <= 0;
    root.querySelector('.kss-next').disabled = view.page >= count - 1;
    root.querySelector('.kss-page-status').textContent = `Page ${view.page + 1} of ${count}`;
  };

  const scheduleNavUpdate = (event) => {
    const view = event.data.view;
    if (!view.navFrame && !view.laying) {
      view.navFrame = requestAnimationFrame(() => { view.navFrame = 0; updateNav(event); });
    }
  };

  const layoutSettings = (event) => {
    const view = event.data.view;
    const root = view.root;

    // Hidden (the window is closed): nothing to measure; it is laid out when it opens
    if (!root.getClientRects().length) {
      view.key = '';
      return;
    }

    // Not while a field is being used (the on-screen keyboard resizes the window; moving the field would close it)
    const focused = document.activeElement;
    if (focused && root.contains(focused) && focused.matches('input[type="text"], select')) {
      view.pending = true;
      return;
    }

    fitToScreen(root);

    const width = root.clientWidth;
    const height = root.clientHeight;
    const key = `${width}x${height}`;
    if (!width || !height || key === view.key) {
      return;
    }
    view.key = key;
    view.pending = false;
    view.laying = true;

    const keep = view.page;
    root.setAttribute('data-w', width >= WIDE_PX ? 'l' : width >= MEDIUM_PX ? 'm' : 's');
    root.setAttribute('data-h', height < SHORT_PX ? 's' : 'm');

    (width >= MEDIUM_PX && layoutTogether(view, width >= WIDE_PX)) || layoutOneByOne(view);

    goToPage(view, keep, false);
    view.laying = false;
    updateNav(event);
  };

  const scheduleLayout = (event) => {
    const view = event.data.view;
    cancelAnimationFrame(view.layoutFrame);
    view.layoutFrame = requestAnimationFrame(() => layoutSettings(event));
  };

  // ---------------------------------------------------------------------------
  // Drawing the state on both the icon (pedalboard) and the settings page

  const stepsHtml = (steps) => {
    const length = steps.length;
    let html = '';

    for (let i = 0; i < DISPLAYED_STEPS; i++) {
      const classes = [
        `l${steps[i % length]}`,
        i >= length ? 'again' : '',
        i > 0 && i % length === 0 ? 'start' : '',
      ].filter(Boolean).join(' ');
      html += `<i class="${classes}"></i>`;
    }
    return html;
  };

  const setSlider = (input, value, min, max) => {
    input.value = String(value);
    input.style.setProperty('--ks-fill', `${((value - min) / (max - min)) * 100}%`);
  };

  const setText = (el, text) => {
    if (el.textContent !== text) {
      el.textContent = text;
    }
  };

  const drawIcon = (event) => {
    const { state, el } = event.data;
    const pattern = PATTERNS[state.pattern] || PATTERNS[0];
    const steps = stepsOf(state, state.pattern);

    el.root.classList.toggle('is-bypassed', state.bypassed);
    setText(el.state, state.bypassed ? 'Bypassed' : 'On');
    setText(el.name, pattern.name);

    if (el.steps.getAttribute('data-steps') !== steps) {
      el.steps.setAttribute('data-steps', steps);
      el.steps.innerHTML = stepsHtml(steps);
      el.steps.setAttribute('aria-label', `The steps of ${pattern.name}: ${steps.split('').map((level) => LEVEL_NAMES[level].toLowerCase()).join(', ')}`);
    }

    el.bpm.innerHTML = `<b>${state.bpm}</b> BPM`;
    el.segments.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-speed')) === state.speed)));
    setSlider(el.depth, state.depth, 0, 100);
    setText(el.depthValue, `${Math.round(state.depth)}%`);
    setSlider(el.edge, edgeToSlider(state.edge), 0, EDGE_SLIDER);
    setText(el.edgeValue, edgeText(state.edge));
  };

  const drawSettings = (event) => {
    const { state, el } = event.data;
    const custom = state.custom;

    setText(el.now, (PATTERNS[state.pattern] || PATTERNS[0]).name);

    el.cells.forEach((cell, i) => {
      const level = custom.steps[i];
      cell.button.setAttribute('data-level', String(level));
      cell.button.classList.toggle('is-off', i >= custom.length);
      setText(cell.level, LEVEL_NAMES[level]);
      cell.button.setAttribute('aria-label', `Step ${i + 1}: ${LEVEL_NAMES[level].toLowerCase()}${i >= custom.length ? ', beyond the length, not played' : ''}. Tap to change`);
    });

    setText(el.length, String(custom.length));
    el.beat.innerHTML = `${custom.perBeat}<small>${beatText(custom.perBeat)}</small>`;
    el.use.setAttribute('aria-pressed', String(state.pattern === CUSTOM));
    setText(el.use, state.pattern === CUSTOM ? 'Playing' : 'Play this pattern');
    el.lengthDown.disabled = custom.length <= 1;
    el.lengthUp.disabled = custom.length >= CUSTOM_STEPS;
    el.beatDown.disabled = custom.perBeat <= 1;
    el.beatUp.disabled = custom.perBeat >= CUSTOM_MAX_PER_BEAT;
  };

  const draw = (event) => {
    drawIcon(event);
    drawSettings(event);
  };

  // ---------------------------------------------------------------------------
  // Events from the user

  const holdToRepeat = (event, button, step) => {
    const stop = () => {
      clearTimeout(event.data.state.timers.hold);
      clearInterval(event.data.state.timers.holdRepeat);
    };

    button.addEventListener('pointerdown', () => {
      stop();
      button.setAttribute('data-pointer', '1');
      step();
      event.data.state.timers.hold = setTimeout(() => {
        event.data.state.timers.holdRepeat = setInterval(step, HOLD_EVERY_MS);
      }, HOLD_DELAY_MS);
    });
    ['pointerup', 'pointercancel', 'pointerleave'].forEach((type) => button.addEventListener(type, stop));

    // A click that follows a press was already handled; one without (the keyboard) is a single step
    button.addEventListener('click', () => {
      if (button.getAttribute('data-pointer')) {
        button.removeAttribute('data-pointer');
      } else {
        step();
      }
    });
  };

  const openSettings = (event) => {
    // MOD UI adds its own "settings" action to the plugin icon: the one that opens the settings page
    event.icon.find('.mod-actions .mod-settings').first().trigger('click');
    setTimeout(() => scheduleLayout(event), 0);
  };

  const assignEvents = (event) => {
    const { el } = event.data;
    const state = () => event.data.state;

    // The icon
    el.prev.addEventListener('click', () => setPattern(event, state().pattern - 1));
    el.next.addEventListener('click', () => setPattern(event, state().pattern + 1));
    el.open.addEventListener('click', () => openSettings(event));
    holdToRepeat(event, el.down, () => setBpm(event, state().bpm - 1));
    holdToRepeat(event, el.up, () => setBpm(event, state().bpm + 1));

    el.segments.forEach((button) => button.addEventListener('click', () => setSpeed(event, Number(button.getAttribute('data-speed')))));

    el.depth.addEventListener('input', () => {
      state().depth = Number(el.depth.value);
      sendThrottled(event, 'depth', state().depth);
      draw(event);
    });
    el.edge.addEventListener('input', () => {
      state().edge = Math.round(sliderToEdge(Number(el.edge.value)) * 100) / 100;
      sendThrottled(event, 'edge', state().edge);
      draw(event);
    });

    // The settings page
    el.cells.forEach((cell, i) => cell.button.addEventListener('click', () => cycleStep(event, i)));
    holdToRepeat(event, el.lengthDown, () => setLength(event, state().custom.length - 1));
    holdToRepeat(event, el.lengthUp, () => setLength(event, state().custom.length + 1));
    holdToRepeat(event, el.beatDown, () => setPerBeat(event, state().custom.perBeat - 1));
    holdToRepeat(event, el.beatUp, () => setPerBeat(event, state().custom.perBeat + 1));
    el.use.addEventListener('click', () => setPattern(event, CUSTOM));
    el.from.addEventListener('change', () => {
      if (el.from.value !== '') {
        copyPattern(event, Number(el.from.value));
        el.from.value = '';
      }
    });

    const view = event.data.view;
    event.settings.find('.kss-prev').on('click', () => stepPage(event, -1));
    event.settings.find('.kss-next').on('click', () => stepPage(event, 1));
    event.settings.on('click', '.kss-dot', function () { goToPage(view, Number($(this).attr('data-page')), true); });
    event.settings.on('keydown', (e) => {
      if (e.target.matches && e.target.matches('input, select')) {
        return;
      }
      if (e.key === 'ArrowRight') { stepPage(event, 1); } else if (e.key === 'ArrowLeft') { stepPage(event, -1); }
    });
    // A field gives the window back: the layout that waited for it can run
    event.settings.on('focusout', () => { if (view.pending) { setTimeout(() => scheduleLayout(event), 0); } });
    view.pager.addEventListener('scroll', () => scheduleNavUpdate(event), { passive: true });

    // The page is laid out when it is shown and whenever the window changes
    if (typeof ResizeObserver === 'function') {
      new ResizeObserver(() => scheduleLayout(event)).observe(event.settings[0]);
    } else {
      window.addEventListener('resize', () => scheduleLayout(event));
    }
  };

  const buildSettings = (event) => {
    const settings = event.settings;

    settings.find('.kss-steps').html(range(0, CUSTOM_STEPS - 1).map((i) =>
      `<button type="button" class="kss-cell" data-level="2"><span class="kss-cell-bar" aria-hidden="true"></span>` +
      `<span class="kss-cell-n">${i + 1}</span><span class="kss-cell-level">Pass</span></button>`).join(''));

    settings.find('.kss-from').html('<option value="">Copy from&hellip;</option>' +
      PATTERNS.map((pattern, i) => (pattern.steps ? `<option value="${i}">${pattern.name}</option>` : '')).join(''));
  };

  const cacheElements = (event) => {
    const root = event.icon.find('.kill-switch')[0];
    const one = (selector) => root.querySelector(selector);
    const settings = event.settings[0];
    const inSettings = (selector) => settings.querySelector(selector);

    event.data.el = {
      root,
      state: one('.ks-state'),
      name: one('.ks-pattern-name'),
      steps: one('.ks-steps'),
      prev: one('.ks-prev'),
      next: one('.ks-next'),
      open: one('.ks-open'),
      down: one('.ks-bpm-down'),
      up: one('.ks-bpm-up'),
      bpm: one('.ks-bpm'),
      segments: Array.from(root.querySelectorAll('.ks-segment')),
      depth: one('.ks-depth-input'),
      depthValue: one('.ks-depth-value'),
      edge: one('.ks-edge-input'),
      edgeValue: one('.ks-edge-value'),
      // The settings page
      now: inSettings('.kss-now-label'),
      cells: Array.from(settings.querySelectorAll('.kss-cell')).map((button) => ({ button, level: button.querySelector('.kss-cell-level') })),
      length: inSettings('.kss-length'),
      lengthDown: inSettings('.kss-length-down'),
      lengthUp: inSettings('.kss-length-up'),
      beat: inSettings('.kss-beat'),
      beatDown: inSettings('.kss-beat-down'),
      beatUp: inSettings('.kss-beat-up'),
      from: inSettings('.kss-from'),
      use: inSettings('.kss-use'),
    };
  };

  // ---------------------------------------------------------------------------
  // Events from the plugin / MOD UI. Returns whether what is drawn may have changed

  const handlePort = (event, symbol, value) => {
    const state = event.data.state;
    const number = Number(value);
    let match;

    if (symbol === ':bypass') {
      // MOD UI's bypass: 1 = bypassed
      const bypassed = number >= 0.5;
      const changed = state.bypassed !== bypassed;
      state.bypassed = bypassed;
      return changed;
    }

    if (symbol === 'pattern') state.pattern = clamp(Math.round(number), 0, CUSTOM);
    else if (symbol === 'bpm') state.bpm = clamp(Math.round(number), BPM.min, BPM.max);
    else if (symbol === 'speed') state.speed = clamp(Math.round(number), 0, SPEEDS - 1);
    else if (symbol === 'depth') state.depth = clamp(number, 0, 100);
    else if (symbol === 'edge') state.edge = clamp(number, EDGE.min, EDGE.max);
    else if (symbol === 'custom_length') state.custom.length = clamp(Math.round(number), 1, CUSTOM_STEPS);
    else if (symbol === 'custom_steps_per_beat') state.custom.perBeat = clamp(Math.round(number), 1, CUSTOM_MAX_PER_BEAT);
    else if ((match = /^custom_step_(\d+)$/.exec(symbol || '')) && Number(match[1]) >= 1 && Number(match[1]) <= CUSTOM_STEPS) {
      state.custom.steps[Number(match[1]) - 1] = clamp(Math.round(number), 0, LEVELS - 1);
    } else {
      return false;
    }
    return true;
  };

  if (event.type === 'start') {
    event.data.state = newState();
    buildSettings(event);
    cacheElements(event);
    event.data.view = findPanels(event.settings.find('.kss')[0]);
    assignEvents(event);

    for (const port of event.ports) {
      handlePort(event, port.symbol, port.value);
    }
    draw(event);

  } else if (event.data.state && event.type === 'change') {
    if (handlePort(event, event.symbol, event.value)) {
      draw(event);
    }
  }
}
