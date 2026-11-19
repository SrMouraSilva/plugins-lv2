function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.

  // Mirrors PATTERNS in plugins/src/kill-switch.c (and the scale points of `pattern` in kill-switch.ttl): one row of steps
  // for each side, '0' = cut, '1' = half, '2' = the signal passes. The first ten have the same row on both sides (the
  // single-channel kill switch), the next nine a row for each side. The last pattern, Custom, is the one written on the
  // settings page: its steps are the ports custom_left_step_N and custom_right_step_N. Keep them in step
  const both = (name, perBeat, steps) => ({ name, perBeat, left: steps, right: steps });
  const PATTERNS = [
    both('Chop 8ths',           2, '02'),
    both('Chop 16ths',          4, '02'),
    both('Triplet chop',        3, '02'),
    both('Machine gun',         8, '02'),
    both('Syncopated 3-3-2',    2, '02202202'),
    both('Gallop',              4, '0220'),
    both('Morello solo chop',   4, '0202022002022022'),
    both('Morello toggle solo', 4, '0222020200220202'),
    both('Greenwood chunk',     2, '02220222'),
    both('Buckethead burst',    8, '0202020222222222'),
    { name: 'Ping-pong 8ths',   perBeat: 2, left: '20',       right: '02' },
    { name: 'Ping-pong 16ths',  perBeat: 4, left: '20',       right: '02' },
    { name: 'Triplet pong',     perBeat: 3, left: '200',      right: '020' },
    { name: 'Half deaf: left',  perBeat: 2, left: '02',       right: '22' },
    { name: 'Half deaf: right', perBeat: 2, left: '22',       right: '02' },
    { name: 'Gallop sides',     perBeat: 4, left: '2200',     right: '0022' },
    { name: 'Stagger',          perBeat: 4, left: '2002',     right: '0220' },
    { name: 'Seesaw',           perBeat: 2, left: '2210',     right: '0122' },
    { name: 'Syncopated sides', perBeat: 2, left: '20020020', right: '02002002' },
    { name: 'Custom',           perBeat: null, left: null,    right: null },
  ];
  const CUSTOM = PATTERNS.length - 1;          // the value of `pattern` that plays the custom pattern
  const CUSTOM_STEPS = 16;
  const CUSTOM_MAX_PER_BEAT = 8;
  const LEVELS = 3;
  const LEVEL_NAMES = ['Cut', 'Half', 'Pass'];
  const BEAT_NAMES = { 1: 'quarters', 2: '8ths', 3: 'triplets', 4: '16ths', 6: 'sextuplets', 8: '32nds' };
  const SIDES = ['left', 'right'];
  const SPEEDS = 4;
  const BPM = { min: 40, max: 240 };
  const LENGTH = { min: 2, max: 32 };          // beats
  const EDGE = { min: 0.1, max: 20 };          // ms, a logarithmic port: the slider is 0..EDGE_SLIDER
  const EDGE_SLIDER = 1000;
  const DISPLAYED_STEPS = 16;
  const ENDING_CELLS = 48;                     // the columns that draw an ending along its time
  const SEND_EVERY_MS = 40;                    // a slider that is being dragged sends at most this often
  const HOLD_DELAY_MS = 400;                   // holding - or + repeats after this, every HOLD_EVERY_MS
  const HOLD_EVERY_MS = 70;

  // The endings: they play once when the plugin is switched on and finish in silence. Mirrors ending_gate() in
  // kill-switch.c: the gate (0 = cut, 1 = pass) of each side at the time t (seconds since it was switched on) of an ending
  // that lasts `total` seconds, with beats of `beat` seconds. Keep them in step
  const BOUNCE_RATIO = 0.78, BOUNCE_OPEN = 0.55, BOUNCE_MAX_BEATS = 0.35, BOUNCE_END_SECONDS = 0.001;
  const HELICOPTER_GROWTH = 1.12, HELICOPTER_OPEN = 0.5;
  const ENDINGS = [
    { name: 'Bouncing ball', gate: (t, total, beat) => {
      // the gaps shrink by a fixed ratio and pile up at the end: tam---tam--tam-tam-ta-ta-t-t-ttt
      const k = Math.floor(Math.log(1 - t / total) / Math.log(BOUNCE_RATIO));
      const start = total * (1 - Math.pow(BOUNCE_RATIO, k));
      const gap = total * Math.pow(BOUNCE_RATIO, k) * (1 - BOUNCE_RATIO);
      if (gap < BOUNCE_END_SECONDS) return [0, 0];
      return t - start < Math.min(gap * BOUNCE_OPEN, beat * BOUNCE_MAX_BEATS) ? [1, 1] : [0, 0];
    } },
    { name: 'Helicopter', gate: (t, total, beat) => {
      // the blades go to one side, then the other, and the gap between them grows while the sound fades
      const p0 = beat * 0.25;
      const k = Math.max(0, Math.floor(Math.log(1 + (t * (HELICOPTER_GROWTH - 1)) / p0) / Math.log(HELICOPTER_GROWTH)));
      const start = (p0 * (Math.pow(HELICOPTER_GROWTH, k) - 1)) / (HELICOPTER_GROWTH - 1);
      const period = p0 * Math.pow(HELICOPTER_GROWTH, k);
      if (t - start >= period * HELICOPTER_OPEN) return [0, 0];
      const level = 1 - t / total;
      return k % 2 === 0 ? [level, 0] : [0, level];
    } },
    { name: 'Stutter out', gate: (t, total, beat) => {
      // sixteenth notes whose open part shrinks to nothing; the right side is half a period behind the left one
      const period = beat * 0.25;
      const duty = 0.9 * (1 - t / total) + 0.02;
      return [(t % period) / period < duty ? 1 : 0, ((t + period * 0.5) % period) / period < duty ? 1 : 0];
    } },
    { name: 'Side fade', gate: (t, total, beat) => {
      // eighth notes; the left side fades out first, by 60% of the ending, and the right one at the end
      const period = beat * 0.5;
      if ((t % period) / period >= 0.5) return [0, 0];
      return [Math.max(0, 1 - t / total / 0.6), 1 - t / total];
    } },
    { name: 'Ping-pong out', gate: (t, total, beat) => {
      // eighth notes, one to each side in turn, every one quieter than the one before
      const period = beat * 0.5;
      const k = Math.floor(t / period);
      if (t - k * period >= period * 0.6) return [0, 0];
      const level = Math.pow(1 - t / total, 1.5);
      return k % 2 === 0 ? [level, 0] : [0, level];
    } },
  ];

  const stepSymbol = (side, index) => `custom_${side}_step_${index + 1}`;
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
  // State: { mode: 0 patterns / 1 endings, pattern: 0..CUSTOM, ending, bpm, speed: 0..3, length (beats), depth: 0..100,
  //          edge: ms, mono, kill, custom: { length: 1..16, perBeat: 1..8, left: [16], right: [16] => 0..2 },
  //          lastSent: [symbol] => ms, timers }

  const newState = () => ({
    mode: 0, pattern: 0, ending: 0, bpm: 120, speed: 1, length: 8, depth: 100, edge: 1.5, mono: false, kill: false,
    // The defaults of the ports in kill-switch.ttl
    custom: {
      length: 8, perBeat: 2,
      left: [2, 1, 0, 1, 2, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2],
      right: [0, 1, 2, 1, 0, 1, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2],
    },
    lastSent: {}, timers: {},
  });

  // The steps of a pattern as one string of levels for each side: those of a built-in one, or the first `length` of the custom one
  const stepsOf = (state, index) => (index === CUSTOM
    ? { left: state.custom.left.slice(0, state.custom.length).join(''), right: state.custom.right.slice(0, state.custom.length).join('') }
    : { left: PATTERNS[index].left, right: PATTERNS[index].right });

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

  const setMode = (event, mode) => {
    event.data.state.mode = mode;
    send(event, 'mode', mode);
    draw(event);
  };

  // The arrows go through what the family has, and round
  const step = (event, direction) => {
    const state = event.data.state;
    const count = state.mode === 1 ? ENDINGS.length : PATTERNS.length;
    const symbol = state.mode === 1 ? 'ending' : 'pattern';
    const value = ((state[symbol === 'ending' ? 'ending' : 'pattern'] + direction) % count + count) % count;

    state[symbol] = value;
    send(event, symbol, value);
    draw(event);
  };

  // Picks one of the family that is on (a pattern, or an ending)
  const choose = (event, index) => {
    const state = event.data.state;
    const symbol = state.mode === 1 ? 'ending' : 'pattern';

    state[symbol] = index;
    send(event, symbol, index);
    draw(event);
  };

  const setBpm = (event, bpm) => {
    event.data.state.bpm = clamp(Math.round(bpm), BPM.min, BPM.max);
    send(event, 'bpm', event.data.state.bpm);
    draw(event);
  };

  const setLengthBeats = (event, length) => {
    event.data.state.length = clamp(Math.round(length), LENGTH.min, LENGTH.max);
    send(event, 'length', event.data.state.length);
    draw(event);
  };

  const setSpeed = (event, speed) => {
    event.data.state.speed = speed;
    send(event, 'speed', speed);
    draw(event);
  };

  const setMono = (event, mono) => {
    event.data.state.mono = mono;
    send(event, 'mono', mono ? 1 : 0);
    draw(event);
  };

  // The footswitch of the pedal (and its twin on the settings page). The plugin switches itself: a hardware control assigned to
  // Kill is momentary by default, so it chops only while it is held
  const setKill = (event, kill) => {
    event.data.state.kill = kill;
    send(event, 'kill', kill ? 1 : 0);
    draw(event);
  };

  // Writing the custom pattern is meant to be heard: the first edit puts the plugin on the patterns, on Custom
  const editing = (event) => {
    const state = event.data.state;
    if (state.mode !== 0) {
      state.mode = 0;
      send(event, 'mode', 0);
    }
    if (state.pattern !== CUSTOM) {
      state.pattern = CUSTOM;
      send(event, 'pattern', CUSTOM);
    }
  };

  const cycleStep = (event, side, index) => {
    const custom = event.data.state.custom;
    editing(event);
    custom[side][index] = (custom[side][index] + 1) % LEVELS;      // cut -> half -> pass -> cut
    send(event, stepSymbol(side, index), custom[side][index]);
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

  // Copies a built-in pattern (both sides) into the custom one and plays that one
  const copyPattern = (event, index) => {
    const source = PATTERNS[index];
    const custom = event.data.state.custom;
    if (!source || !source.left) {
      return;
    }

    custom.length = source.left.length;
    custom.perBeat = source.perBeat;
    send(event, 'custom_length', custom.length);
    send(event, 'custom_steps_per_beat', custom.perBeat);
    SIDES.forEach((side) => {
      // The steps after the end keep the level of the pass, as in the defaults: they do not play until the length grows
      custom[side] = range(0, CUSTOM_STEPS - 1).map((i) => (i < custom.length ? Number(source[side][i]) : 2));
      custom[side].forEach((level, i) => send(event, stepSymbol(side, i), level));
    });
    // Only now: Custom starts to play with its steps already in place
    editing(event);
    draw(event);
  };

  // ---------------------------------------------------------------------------
  // The question that comes before a built-in pattern becomes a Custom one: the dialog is part of the settings page, so it
  // has the theme of the plugin (and is still there in MOD UI's Performance view)

  const askCustom = (event, opener) => {
    const { state, el } = event.data;
    const pattern = PATTERNS[state.pattern];
    if (state.mode !== 0 || !pattern || !pattern.left) {
      return;
    }

    event.data.asked = { index: state.pattern, opener };
    el.modalText.textContent = `${pattern.name} is a built-in pattern, so its steps stay as they are. Define a Custom pattern that starts as a copy of it, and play that one? You can then tap its steps to change them.`;
    el.modal.hidden = false;
    el.modalOk.focus();
  };

  const closeModal = (event) => {
    const { el, asked } = event.data;
    el.modal.hidden = true;
    event.data.asked = null;
    if (asked && asked.opener && asked.opener.focus) {
      asked.opener.focus();
    }
  };

  const confirmCustom = (event) => {
    const asked = event.data.asked;
    closeModal(event);
    if (asked) {
      copyPattern(event, asked.index);
    }
  };

  // ---------------------------------------------------------------------------
  // The settings page never scrolls vertically: its panels (the pattern editor, the saved presets)
  // are spread over horizontal pages, as few as the screen allows, one swipe (or one of the buttons of the top bar)
  // apart. What fits is measured here instead of left to @media queries: MOD UI's page has no <meta name="viewport">
  // and its body is at least 1024px wide, so a phone lays it out in about 980px and shrinks it to the screen, and a
  // query on the viewport never sees a phone. See docs/custom-settings-pages.md

  const WIDE_PX = 1100;          // from this width, panels can sit side by side
  const MEDIUM_PX = 700;
  const SHORT_PX = 600;          // below this height, the descriptions go and everything gets tighter
  const CELL_MIN_PX = 44;        // the narrowest a step of the editor can be
  const LANE_LABEL_PX = 26;      // the L and the R beside a row of steps: their column and its gap
  const SOUND_TWO_COLUMNS_PX = 420;  // Depth and Edge side by side from this width of the panel

  // The panels, the lists they hold, and what moves them: found once, on 'start'
  const findPanels = (root) => {
    return {
      root,
      pager: root.querySelector('.kss-pager'),
      stock: root.querySelector('.kss-stock'),
      play: root.querySelector('[data-panel="play"]'),
      sound: root.querySelector('[data-panel="sound"]'),
      presets: { panel: root.querySelector('[data-panel="presets"]') },
      led: root.querySelector('.kss-led'),
      ledCells: Array.from(root.querySelectorAll('.kss-led-cells')),
      columns: 8,
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
    view.stock.append(view.play, view.sound, view.presets.panel);
    view.pager.textContent = '';
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

  // The LEDs fill a row at most `view.columns` wide; fewer of them (a short pattern) share the width among themselves
  const applyColumns = (view, shown) => view.ledCells.forEach((body) => body.style.setProperty('--kss-cols', Math.min(view.columns, shown)));

  // What depends on the width of a panel (it must be on a page to be measured): the columns of the LEDs (16, 8 or 4,
  // so that every line is full), and Depth and Edge side by side
  const sizeAll = (view) => {
    const room = view.led.clientWidth - LANE_LABEL_PX;
    const gap = px(getComputedStyle(view.ledCells[0]).columnGap);
    let columns = CUSTOM_STEPS;
    // (a phone gives a step a little less than 44px of width, which its 44px of height makes up for)
    const narrowest = view.root.getAttribute('data-w') === 's' ? CELL_MIN_PX - 10 : CELL_MIN_PX;
    while (columns > 4 && (room - gap * (columns - 1)) / columns < narrowest) {
      columns /= 2;
    }
    view.columns = columns;
    applyColumns(view, CUSTOM_STEPS);

    // Depth and Edge side by side need room for a name, a value and a button each
    const sound = view.sound.querySelector('.kss-sound');
    sound.classList.toggle('is-narrow', sound.clientWidth < SOUND_TWO_COLUMNS_PX);
  };

  const fits = (page) => [page, ...page.querySelectorAll('.kss-panel, .kss-stack, .kss-row')]
    .every((el) => el.scrollHeight <= el.clientHeight + 1 && el.scrollWidth <= el.clientWidth + 1);

  // What goes on which page, from the one that keeps the most together to the one panel to a page. The choice of pattern and
  // the LEDs that show it (and, for Custom, write it) are one panel, always together; tempo and feel sit beside it, and the
  // saved presets beside them, or follow
  const arrangements = (view, width) => {
    const { play, sound, presets } = view;
    const row = (...nodes) => { const el = element('kss-row'); el.append(...nodes); return el; };
    return [
      ...(width >= WIDE_PX ? [() => [[row(play, sound, presets.panel)]]] : []),
      ...(width >= MEDIUM_PX ? [() => [[row(play, sound)], [presets.panel]]] : []),
      () => [[play], [sound], [presets.panel]],
    ];
  };

  // Tries every arrangement with all the patterns at hand (a grid of buttons) and, only when none fits, again with the
  // arrows that show one at a time ("< name >"), which take much less room. The last arrangement is also the last resort:
  // it is used with the arrows even when it does not fit
  const layoutPages = (view, width) => {
    const list = arrangements(view, width);
    const attempts = [
      ...['grid', 'stepper'].flatMap((choice) => list.map((make) => ({ choice, make }))),
    ];

    for (const { choice, make } of attempts) {
      gather(view);
      view.root.setAttribute('data-choice', choice);
      const pages = setPages(view, make());
      sizeAll(view);
      if (pages.every(fits)) {
        return pages;
      }
    }
    return null;
  };

  const layoutOneByOne = (view) => {
    gather(view);
    view.root.setAttribute('data-choice', 'stepper');
    const pages = setPages(view, [view.play, view.sound, view.presets.panel].map((panel) => [panel]));
    // What still does not fit scrolls, rather than being cut off
    pages.forEach((page) => page.classList.toggle('is-scroll', !fits(page)));
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

    // The tools of Custom are only shown while Custom plays, but the room for them is kept: measure as if they were there
    const custom = root.getAttribute('data-custom');
    // ... and with every LED of the longest pattern
    root.setAttribute('data-custom', '1');
    root.setAttribute('data-measure', '1');
    layoutPages(view, width) || layoutOneByOne(view);
    root.removeAttribute('data-measure');
    root.setAttribute('data-custom', custom || '0');
    drawSettings(event);

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

  // An ending along its time: the columns are slices of it, as bright as the gate is open (the highest in the slice)
  const endingHtml = (state, side) => {
    const ending = ENDINGS[state.ending] || ENDINGS[0];
    const beat = 60 / state.bpm;
    const total = state.length * beat;
    const SUBSAMPLES = 24;
    let html = '';

    for (let i = 0; i < ENDING_CELLS; i++) {
      let level = 0;
      for (let s = 0; s < SUBSAMPLES; s++) {
        level = Math.max(level, ending.gate(((i + (s + 0.5) / SUBSAMPLES) / ENDING_CELLS) * total, total, beat)[side]);
      }
      html += `<i style="opacity:${(0.14 + 0.86 * level).toFixed(2)}" class="${level > 0.02 ? 'l2' : 'l0'}"></i>`;
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
    const endings = state.mode === 1;
    const pattern = PATTERNS[state.pattern] || PATTERNS[0];
    const name = endings ? (ENDINGS[state.ending] || ENDINGS[0]).name : pattern.name;

    el.root.classList.toggle('is-idle', !state.kill);
    el.root.setAttribute('data-mode', String(state.mode));
    setText(el.state, state.kill ? 'On' : 'Off');
    setText(el.name, name);
    el.foot.setAttribute('aria-pressed', String(state.kill));
    el.modes.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-mode')) === state.mode)));
    el.mono.setAttribute('aria-pressed', String(state.mono));
    el.monoSettings.setAttribute('aria-pressed', String(state.mono));

    // What the two lanes draw is only built again when it changes
    el.lanes.forEach((lane) => drawLane(state, lane, lane.querySelector('.ks-steps'), name));

    el.bpm.innerHTML = `<b>${state.bpm}</b> BPM`;
    el.lengthValue.innerHTML = `<b>${state.length}</b> beats`;
    el.segments.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-speed')) === state.speed)));
    setSlider(el.depth, state.depth, 0, 100);
    setText(el.depthValue, `${Math.round(state.depth)}%`);
    setSlider(el.edge, edgeToSlider(state.edge), 0, EDGE_SLIDER);
    setText(el.edgeValue, edgeText(state.edge));
  };

  // The lanes of the pedal, of the glance and of the endings draw the same: only built again when what they draw changes
  const drawLane = (state, lane, cells, name) => {
    const endings = state.mode === 1;
    const side = lane.getAttribute('data-side') === 'l' ? 'left' : 'right';
    const key = endings ? `e${state.ending}/${state.length}/${state.bpm}` : `p${stepsOf(state, state.pattern)[side]}`;

    if (cells.getAttribute('data-key') !== key) {
      cells.setAttribute('data-key', key);
      cells.setAttribute('data-kind', endings ? 'ending' : 'pattern');
      cells.style.gridTemplateColumns = `repeat(${endings ? ENDING_CELLS : DISPLAYED_STEPS}, minmax(0, 1fr))`;
      cells.innerHTML = endings ? endingHtml(state, side === 'left' ? 0 : 1) : stepsHtml(stepsOf(state, state.pattern)[side]);
      cells.setAttribute('aria-label', `${side === 'left' ? 'The left' : 'The right'} side of ${name}`);
    }
  };

  // The LEDs of the big preview: the steps of the pattern that plays, a level to each: cut (dark), half (lit from the bottom up to
  // half) or pass (lit). Twice the length of the pattern is shown (at most sixteen): the second run, drawn as outlines, is the
  // pattern coming round again, and a tap on it changes the step it repeats
  const drawLeds = (state, el, view) => {
    const custom = state.custom;
    const writing = state.pattern === CUSTOM;
    const built = PATTERNS[state.pattern] || PATTERNS[0];
    const steps = writing ? null : { left: built.left, right: built.right };
    const length = writing ? custom.length : built.left.length;
    const perBeat = writing ? custom.perBeat : built.perBeat;
    const shown = Math.min(length * 2, CUSTOM_STEPS);

    applyColumns(view, shown);
    SIDES.forEach((side) => {
      el.cells[side].forEach((cell, i) => {
        const source = i % length;
        const level = writing ? custom[side][source] : Number(steps[side][source]);
        cell.setAttribute('data-level', String(level));
        cell.setAttribute('data-source', String(source));
        cell.classList.toggle('is-unused', i >= shown);
        cell.classList.toggle('again', i >= length);
        cell.classList.toggle('is-beat', i % perBeat === 0);
        cell.setAttribute('aria-label', `${side} step ${source + 1}${i >= length ? ' (again)' : ''}: ${LEVEL_NAMES[level].toLowerCase()}${writing
          ? '. Tap to change'
          : '. A built-in pattern: tap to define a Custom one from it'}`);
      });
    });
    setText(el.ledBeat, ` · ${beatText(perBeat)}`);
  };

  const drawSettings = (event) => {
    const { state, el } = event.data;
    const custom = state.custom;
    const endings = state.mode === 1;
    const playing = !endings && state.pattern === CUSTOM;
    const family = endings ? ENDINGS : PATTERNS;
    const current = endings ? state.ending : state.pattern;
    const name = (family[current] || family[0]).name;
    const kind = endings ? 'Ending' : 'Pattern';

    // What is playing, small, on every page
    el.settingsRoot.setAttribute('data-mode', String(state.mode));
    el.settingsRoot.setAttribute('data-custom', playing ? '1' : '0');
    el.settingsRoot.classList.toggle('is-idle', !state.kill);
    setText(el.glanceKind, kind);
    setText(el.glanceName, name);
    el.glance.setAttribute('aria-label', `${kind}: ${name}`);
    el.glanceLanes.forEach(({ lane, cells }) => drawLane(state, lane, cells, name));

    // The choice: the whole family to pick from, or one at a time with the arrows
    el.modesSettings.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-mode')) === state.mode)));
    setText(el.playKind, kind);
    setText(el.playName, name);
    setText(el.playHint, endings
      ? 'An ending plays once, as soon as you switch the effect on, and ends in silence. Its length is set under Tempo and feel.'
      : 'The rhythm of the two sides. Custom is the one you define yourself.');
    if (el.choices.getAttribute('data-family') !== String(state.mode)) {
      el.choices.setAttribute('data-family', String(state.mode));
      el.choices.setAttribute('aria-label', endings ? 'Endings' : 'Patterns');
      el.choices.innerHTML = family.map((item, i) => `<button type="button" class="kss-choice" role="radio" data-index="${i}" aria-checked="false">${item.name}</button>`).join('');
    }
    Array.from(el.choices.children).forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-index')) === current)));
    setText(el.pickLabel, name);
    setText(el.pickCount, `${current + 1} of ${family.length}`);

    // What each side does: LEDs to tap (patterns) or the ending along its time
    el.ledRoot.setAttribute('data-playing', playing ? '1' : '0');
    if (endings) {
      el.endingLanes.forEach(({ lane, cells }) => drawLane(state, lane, cells, name));
      setText(el.ledTitle, 'Ending');
      setText(el.ledBeat, '');
      setText(el.ledHint, 'The level of each side along the ending. Endings are not written, only chosen.');
    } else {
      drawLeds(state, el, event.data.view);
      setText(el.ledTitle, playing ? 'Your pattern' : 'Preview');
      setText(el.ledHint, playing ? 'Tap a step: cut, then half, then pass.' : 'Tap a step to define your own pattern from this one. An outlined step is the pattern coming round again.');
    }

    // Tempo and feel
    setText(el.bpmValue, String(state.bpm));
    el.bpmInput.value = String(state.bpm);
    el.bpmInput.style.setProperty('--ks-fill', `${((state.bpm - BPM.min) / (BPM.max - BPM.min)) * 100}%`);
    el.segmentsSettings.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-speed')) === state.speed)));
    setText(el.endingLength, String(state.length));
    el.endingLengthDown.disabled = state.length <= LENGTH.min;
    el.endingLengthUp.disabled = state.length >= LENGTH.max;
    setSlider(el.depthSettings, state.depth, 0, 100);
    setText(el.depthValueSettings, `${Math.round(state.depth)}%`);
    setSlider(el.edgeSettings, edgeToSlider(state.edge), 0, EDGE_SLIDER);
    setText(el.edgeValueSettings, edgeText(state.edge));

    setText(el.power, state.kill ? 'Kill on' : 'Kill off');
    el.power.setAttribute('aria-pressed', String(state.kill));

    // The length of the Custom pattern, only shown while it plays
    setText(el.length, String(custom.length));
    el.beat.innerHTML = `${custom.perBeat}<small>${beatText(custom.perBeat)}</small>`;
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

  const holdToRepeat = (event, button, action) => {
    const stop = () => {
      clearTimeout(event.data.state.timers.hold);
      clearInterval(event.data.state.timers.holdRepeat);
    };

    button.addEventListener('pointerdown', () => {
      stop();
      button.setAttribute('data-pointer', '1');
      action();
      event.data.state.timers.hold = setTimeout(() => {
        event.data.state.timers.holdRepeat = setInterval(action, HOLD_EVERY_MS);
      }, HOLD_DELAY_MS);
    });
    ['pointerup', 'pointercancel', 'pointerleave'].forEach((type) => button.addEventListener(type, stop));

    // A click that follows a press was already handled; one without (the keyboard) is a single step
    button.addEventListener('click', () => {
      if (button.getAttribute('data-pointer')) {
        button.removeAttribute('data-pointer');
      } else {
        action();
      }
    });
  };

  const openSettings = (event) => {
    // MOD UI adds its own "settings" action to the plugin icon: the one that opens the settings page
    event.icon.find('.mod-actions .mod-settings').first().trigger('click');
    setTimeout(() => scheduleLayout(event), 0);
  };

  // MOD UI's Performance view shows a second copy of the settings template (rendered with `isPerformanceView`), which no
  // script ever sees: it would stay the bare HTML, never built. While that view shows this plugin, the live page
  // (event.settings, the one this script builds and updates) takes the copy's place. MOD UI keeps the live page hidden in
  // <body>, as its own window: it goes back there when the view closes or shows another plugin, or that window would not open.
  // `data-performance` lets the stylesheet drop what makes no sense in the view (the Close button).
  const adoptInPerformanceView = (event) => {
    const host = document.querySelector('.js-performance-plugin-settings');
    const view = document.getElementById('performance');
    if (!host || !view || typeof MutationObserver !== 'function') return;
    const live = event.settings[0];
    const instance = live.getAttribute('mod-instance');
    let home = null;

    const release = () => {
      if (!live.hasAttribute('data-performance')) return;
      live.removeAttribute('data-performance');
      live.style.display = 'none';
      if (home) home.appendChild(live);
    };
    new MutationObserver(() => {
      const copy = Array.from(host.children).find((el) => el !== live && el.getAttribute('mod-instance') === instance);
      if (copy) {
        if (!live.hasAttribute('data-performance')) home = live.parentNode;
        copy.replaceWith(live);
        live.style.display = 'block';
        live.setAttribute('data-performance', '');
      } else if (live.parentNode !== host) {
        release();
      }
    }).observe(host, { childList: true });
    new MutationObserver(() => {
      if (getComputedStyle(view).display === 'none') release();
    }).observe(view, { attributes: true, attributeFilter: ['class', 'style'] });
  };

  const assignEvents = (event) => {
    const { el } = event.data;
    const state = () => event.data.state;

    // The icon
    el.prev.addEventListener('click', () => step(event, -1));
    el.next.addEventListener('click', () => step(event, 1));
    el.open.addEventListener('click', () => openSettings(event));
    el.mono.addEventListener('click', () => setMono(event, !state().mono));
    el.monoSettings.addEventListener('click', () => setMono(event, !state().mono));
    el.foot.addEventListener('click', () => setKill(event, !state().kill));
    el.modesSettings.forEach((button) => button.addEventListener('click', () => setMode(event, Number(button.getAttribute('data-mode')))));
    el.modes.forEach((button) => button.addEventListener('click', () => setMode(event, Number(button.getAttribute('data-mode')))));
    holdToRepeat(event, el.down, () => setBpm(event, state().bpm - 1));
    holdToRepeat(event, el.up, () => setBpm(event, state().bpm + 1));
    holdToRepeat(event, el.lengthBeatsDown, () => setLengthBeats(event, state().length - 1));
    holdToRepeat(event, el.lengthBeatsUp, () => setLengthBeats(event, state().length + 1));

    el.segments.forEach((button) => button.addEventListener('click', () => setSpeed(event, Number(button.getAttribute('data-speed')))));

    // The choice, the LEDs and the controls of the settings page: the same actions as on the pedal
    el.choices.addEventListener('click', (e) => {
      const button = e.target.closest('button[data-index]');
      if (button) { choose(event, Number(button.getAttribute('data-index'))); }
    });
    el.pickPrev.addEventListener('click', () => step(event, -1));
    el.pickNext.addEventListener('click', () => step(event, 1));
    el.power.addEventListener('click', () => setKill(event, !state().kill));
    holdToRepeat(event, el.bpmDown, () => setBpm(event, state().bpm - 1));
    holdToRepeat(event, el.bpmUp, () => setBpm(event, state().bpm + 1));
    el.bpmInput.addEventListener('input', () => setBpm(event, Number(el.bpmInput.value)));
    holdToRepeat(event, el.endingLengthDown, () => setLengthBeats(event, state().length - 1));
    holdToRepeat(event, el.endingLengthUp, () => setLengthBeats(event, state().length + 1));
    el.segmentsSettings.forEach((button) => button.addEventListener('click', () => setSpeed(event, Number(button.getAttribute('data-speed')))));
    el.depthSettings.addEventListener('input', () => {
      state().depth = Number(el.depthSettings.value);
      sendThrottled(event, 'depth', state().depth);
      draw(event);
    });
    el.edgeSettings.addEventListener('input', () => {
      state().edge = Math.round(sliderToEdge(Number(el.edgeSettings.value)) * 100) / 100;
      sendThrottled(event, 'edge', state().edge);
      draw(event);
    });

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

    // The LEDs: a step of Custom goes to the next level; a step of a built-in pattern asks whether to make a Custom one
    el.ledSteps.addEventListener('click', (e) => {
      const cell = e.target.closest('.kss-cell');
      if (!cell || state().mode !== 0) {
        return;
      }
      if (state().pattern === CUSTOM) {
        cycleStep(event, cell.getAttribute('data-side') === 'l' ? 'left' : 'right', Number(cell.getAttribute('data-source')));
      } else {
        askCustom(event, cell);
      }
    });
    holdToRepeat(event, el.lengthDown, () => setLength(event, state().custom.length - 1));
    holdToRepeat(event, el.lengthUp, () => setLength(event, state().custom.length + 1));
    holdToRepeat(event, el.beatDown, () => setPerBeat(event, state().custom.perBeat - 1));
    holdToRepeat(event, el.beatUp, () => setPerBeat(event, state().custom.perBeat + 1));

    // The dialog: Escape and a tap outside it say no, and the keyboard stays inside it
    el.modalOk.addEventListener('click', () => confirmCustom(event));
    el.modalCancel.addEventListener('click', () => closeModal(event));
    el.modalScrim.addEventListener('click', () => closeModal(event));
    el.modal.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        e.stopPropagation();
        closeModal(event);
      } else if (e.key === 'Tab') {
        const first = el.modalCancel;
        const last = el.modalOk;
        if (e.shiftKey && document.activeElement === first) { e.preventDefault(); last.focus(); }
        else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
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
      if (!el.modal.hidden) {
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

    adoptInPerformanceView(event);
  };

  const buildSettings = (event) => {
    const settings = event.settings;

    SIDES.forEach((side) => {
      settings.find(`.kss-led-cells[data-side="${side[0]}"]`).html(range(0, CUSTOM_STEPS - 1).map((i) =>
        `<button type="button" class="kss-cell" data-side="${side[0]}" data-step="${i}" data-n="${i + 1}" data-level="2"></button>`).join(''));
    });
  };

  const cacheElements = (event) => {
    const root = event.icon.find('.kill-switch')[0];
    const one = (selector) => root.querySelector(selector);
    const settings = event.settings[0];
    const inSettings = (selector) => settings.querySelector(selector);
    const cellsOf = (side) => Array.from(settings.querySelectorAll(`.kss-led-cells[data-side="${side[0]}"] .kss-cell`));
    const lanesOf = (selector) => Array.from(settings.querySelectorAll(selector)).map((lane) => ({ lane, cells: lane.querySelector('[class$="-steps"]') }));

    event.data.el = {
      root,
      state: one('.ks-state'),
      name: one('.ks-pattern-name'),
      lanes: Array.from(root.querySelectorAll('.ks-lane')),
      prev: one('.ks-prev'),
      next: one('.ks-next'),
      open: one('.ks-open'),
      mono: one('.ks-mono'),
      foot: one('.ks-foot'),
      modes: Array.from(root.querySelectorAll('.ks-mode')),
      down: one('.ks-bpm-down'),
      up: one('.ks-bpm-up'),
      bpm: one('.ks-bpm'),
      lengthBeatsDown: one('.ks-length-down'),
      lengthBeatsUp: one('.ks-length-up'),
      lengthValue: one('.ks-length-value'),
      segments: Array.from(root.querySelectorAll('.ks-segment')),
      depth: one('.ks-depth-input'),
      depthValue: one('.ks-depth-value'),
      edge: one('.ks-edge-input'),
      edgeValue: one('.ks-edge-value'),
      // The settings page
      settingsRoot: inSettings('.kss'),
      glance: inSettings('.kss-glance'),
      glanceKind: inSettings('.kss-glance-kind'),
      glanceName: inSettings('.kss-glance-name'),
      glanceLanes: lanesOf('.kss-glance .kss-lane'),
      monoSettings: inSettings('.kss-mono'),
      power: inSettings('.kss-power'),
      modesSettings: Array.from(settings.querySelectorAll('.kss-mode')),
      playKind: inSettings('.kss-play-kind'),
      playName: inSettings('.kss-play-name'),
      playHint: inSettings('.kss-play-hint'),
      choices: inSettings('.kss-choices'),
      pickPrev: inSettings('.kss-pick-prev'),
      pickNext: inSettings('.kss-pick-next'),
      pickLabel: inSettings('.kss-pick-label'),
      pickCount: inSettings('.kss-pick-count'),
      ledRoot: inSettings('.kss-led'),
      ledSteps: inSettings('.kss-led-steps'),
      ledHint: inSettings('.kss-led-hint'),
      ledTitle: inSettings('.kss-led-title'),
      ledBeat: inSettings('.kss-led-beat'),
      cells: { left: cellsOf('left'), right: cellsOf('right') },
      endingLanes: lanesOf('.kss-led-ending .kss-lane'),
      length: inSettings('.kss-length'),
      lengthDown: inSettings('.kss-length-down'),
      lengthUp: inSettings('.kss-length-up'),
      beat: inSettings('.kss-beat'),
      beatDown: inSettings('.kss-beat-down'),
      beatUp: inSettings('.kss-beat-up'),
      bpmValue: inSettings('.kss-bpm b'),
      bpmDown: inSettings('.kss-bpm-down'),
      bpmUp: inSettings('.kss-bpm-up'),
      bpmInput: inSettings('.kss-bpm-input'),
      segmentsSettings: Array.from(settings.querySelectorAll('.kss-segment')),
      endingLength: inSettings('.kss-ending-length b'),
      endingLengthDown: inSettings('.kss-ending-length-down'),
      endingLengthUp: inSettings('.kss-ending-length-up'),
      depthSettings: inSettings('.kss-depth-input'),
      depthValueSettings: inSettings('.kss-depth-value'),
      edgeSettings: inSettings('.kss-edge-input'),
      edgeValueSettings: inSettings('.kss-edge-value'),
      modal: inSettings('.kss-modal'),
      modalScrim: inSettings('.kss-modal-scrim'),
      modalText: inSettings('.kss-modal-text'),
      modalOk: inSettings('.kss-modal-ok'),
      modalCancel: inSettings('.kss-modal-cancel'),
    };
  };

  // ---------------------------------------------------------------------------
  // Events from the plugin / MOD UI. Returns whether what is drawn may have changed

  const handlePort = (event, symbol, value) => {
    const state = event.data.state;
    const number = Number(value);
    let match;

    // (MOD UI's own ':bypass' is the host's: the plugin does not draw it)
    if (symbol === 'mode') state.mode = clamp(Math.round(number), 0, 1);
    else if (symbol === 'pattern') state.pattern = clamp(Math.round(number), 0, CUSTOM);
    else if (symbol === 'ending') state.ending = clamp(Math.round(number), 0, ENDINGS.length - 1);
    else if (symbol === 'bpm') state.bpm = clamp(Math.round(number), BPM.min, BPM.max);
    else if (symbol === 'speed') state.speed = clamp(Math.round(number), 0, SPEEDS - 1);
    else if (symbol === 'length') state.length = clamp(Math.round(number), LENGTH.min, LENGTH.max);
    else if (symbol === 'depth') state.depth = clamp(number, 0, 100);
    else if (symbol === 'edge') state.edge = clamp(number, EDGE.min, EDGE.max);
    else if (symbol === 'mono') state.mono = number >= 0.5;
    else if (symbol === 'kill') state.kill = number >= 0.5;
    else if (symbol === 'custom_length') state.custom.length = clamp(Math.round(number), 1, CUSTOM_STEPS);
    else if (symbol === 'custom_steps_per_beat') state.custom.perBeat = clamp(Math.round(number), 1, CUSTOM_MAX_PER_BEAT);
    else if ((match = /^custom_(left|right)_step_(\d+)$/.exec(symbol || '')) && Number(match[2]) >= 1 && Number(match[2]) <= CUSTOM_STEPS) {
      state.custom[match[1]][Number(match[2]) - 1] = clamp(Math.round(number), 0, LEVELS - 1);
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
