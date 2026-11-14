function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.
  const PRESETS = 4;        // one footswitch each
  const OUTPUTS = 4;        // CV outputs, and so DIP switches per preset
  const URI = 'http://srmourasilva.github.io/plugins/tetr4-switch';
  const LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;
  // The host answers a click some time later, and several clicks can be in flight: the answers to the previous
  // ones must not flicker the display. A click is "in flight" until the host answers it, or this long passes with
  // the host quiet (MOD UI sends nothing when a click does not change the port, so some are never answered)
  const SETTLE_MS = 800;
  const IN_FLIGHT_MS = 2000;

  const labelUri = (preset) => `${URI}#preset_label_${preset}`;
  const dipSymbol = (preset, output) => `Preset_${preset}_output_${output}`;
  const range = (from, to) => Array.from({ length: to - from + 1 }, (_, i) => from + i);

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  // ---------------------------------------------------------------------------
  // State: { active: 1..4, dips: [preset][output] => bool, labels: [preset] => string,
  //          inFlight: [{preset, at}] the clicks the host has not answered yet, hostPreset: what the host last said }

  const newState = () => ({
    active: 1,
    // Same defaults as the TTL: preset N turns on output N
    dips: range(1, PRESETS).map((p) => range(1, OUTPUTS).map((o) => p === o)),
    labels: range(1, PRESETS).map((p) => `Preset ${p}`),
    inFlight: [],
    hostPreset: 1,
  });

  const labelOf = (state, preset) => state.labels[preset - 1] || `Preset ${preset}`;

  // ---------------------------------------------------------------------------
  // Building the settings page (the pedalboard icon is static HTML)

  const cardHtml = (preset) => {
    const dips = range(1, OUTPUTS).map((output) =>
      `<button type="button" role="switch" aria-checked="false" class="t4s-dip" data-preset="${preset}" data-output="${output}" ` +
      `aria-label="Preset ${preset}, CV output ${output}" title="Preset ${preset} - Output ${output}">` +
      `<span class="t4s-dip-track"></span><span class="t4s-dip-num">CV ${output}</span></button>`
    ).join('');

    return `<div class="t4s-card p${preset}" data-preset="${preset}">` +
      `<button type="button" class="t4s-pad" data-preset="${preset}" aria-pressed="false" title="Make preset ${preset} the active preset"><b>${preset}</b></button>` +
      `<input type="text" class="t4s-label" data-preset="${preset}" maxlength="${LABEL_MAX_LENGTH}" autocomplete="off" autocapitalize="off" spellcheck="false" ` +
      `aria-label="Name of preset ${preset}" value="Preset ${preset}" />` +
      `<div class="t4s-dips" role="group" aria-label="CV outputs on in preset ${preset}">${dips}</div>` +
      `</div>`;
  };

  const buildSettings = (settings) => {
    settings.find('.t4s-grid').html(range(1, PRESETS).map(cardHtml).join(''));
  };

  // ---------------------------------------------------------------------------
  // The settings page never scrolls vertically: its panels (scenes, hardware, saved presets) are spread over
  // horizontal pages, as few as the screen allows, one swipe (or one of the buttons of the top bar) apart.
  //
  // What fits is measured here instead of left to @media queries. MOD UI's page has no <meta name="viewport"> and its
  // body is at least 1024px wide, so a phone lays it out in about 980px and shrinks it to the screen: a query on
  // the viewport never sees a phone. The script measures the box the page is drawn in (fitToScreen makes that box
  // the size of the real screen) and says what it found with data-w / data-h on the root, for the stylesheet.

  const WIDE_PX = 1100;          // from this width, panels can sit side by side
  const MEDIUM_PX = 700;
  const SHORT_PX = 600;          // below this height, the descriptions go and everything gets tighter
  const CARD_MIN_PX = 196;       // the narrowest a preset card can be: its four switches still have room
  const CARD_ROW_PX = 330;       // from this width, a card puts its name beside the switches instead of above them
  const TILE_MIN_PX = 150;       // the hardware assignment tiles
  const TILE_MAX_COLUMNS = 4;

  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const px = (value) => parseFloat(value) || 0;
  const element = (className) => {
    const el = document.createElement('div');
    el.className = className;
    return el;
  };

  // The panels, the lists they hold, and what moves them: found once, on 'start'
  const findPanels = (root) => {
    // minTile: the narrowest an item can be (sets the columns), evenly: no orphan on the last line (4 cards: 1, 2 or 4 columns)
    const part = (panel, bodySelector, itemsSelector, options) => {
      const body = bodySelector && panel.querySelector(bodySelector);
      return {
        panel, body,
        items: body ? Array.from(body.querySelectorAll(itemsSelector)) : [],
        columns: 1,
        ...options,
      };
    };

    return {
      root,
      pager: root.querySelector('.t4s-pager'),
      stock: root.querySelector('.t4s-stock'),
      scenes: part(root.querySelector('[data-panel="scenes"]'), '.t4s-grid', '.t4s-card', { minTile: CARD_MIN_PX, evenly: true, cards: true }),
      // Hardware is told apart by purpose: the preset buttons, the Current preset control, the screen pop-ups
      hardware: Array.from(root.querySelectorAll('[data-panel^="hardware"]')).map((panel) => part(panel, '.t4s-assign', 'li', { minTile: TILE_MIN_PX })),
      presets: part(root.querySelector('[data-panel="presets"]')),
      page: 0,
      key: '',
      pending: false,
    };
  };

  // MOD UI's page has no viewport meta tag, so a phone or a tablet shows it as a ~980px page shrunk to the screen.
  // This draws the page in the size of the screen instead: its box is made as many CSS pixels wide as the screen
  // has, and scaled up to cover the window. Only touch screens that really are shrunk are touched: a desktop
  // browser (zoomed in or out) is left alone. Returns nothing; the box is measured afterwards (clientWidth)
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
    view.stock.append(view.scenes.panel, ...view.hardware.map((part) => part.panel), view.presets.panel);
    view.scenes.body.append(...view.scenes.items);
    view.hardware.forEach((part) => part.body.append(...part.items));
    view.pager.textContent = '';
    view.root.querySelectorAll('.t4s-range').forEach((range) => { range.textContent = ''; });
  };

  const setPages = (view, groups) => {
    view.pager.textContent = '';
    return groups.map((nodes) => {
      const page = element('t4s-page');
      page.append(...nodes);
      view.pager.append(page);
      return page;
    });
  };

  // What depends on the width of a panel (it must be on a page to be measured): the columns of the cards and of the
  // hardware tiles (never more columns than items) and, for the cards, whether a name sits beside its switches
  const sizeTiles = (part) => {
    const list = part.body;
    const gap = px(getComputedStyle(list).columnGap);
    let columns = clamp(Math.floor((list.clientWidth + gap) / (part.minTile + gap)), 1, Math.min(TILE_MAX_COLUMNS, part.items.length));

    while (part.evenly && part.items.length % columns) {
      columns--;
    }

    part.columns = columns;
    list.style.setProperty('--t4s-cols', columns);

    if (part.cards) {
      list.classList.toggle('is-row', (list.clientWidth - gap * (columns - 1)) / columns >= CARD_ROW_PX);
    }
  };

  const sizeAll = (view) => {
    sizeTiles(view.scenes);
    view.hardware.forEach(sizeTiles);
  };

  const fits = (page) => [page, ...page.querySelectorAll('.t4s-panel, .t4s-stack, .t4s-row')]
    .every((el) => el.scrollHeight <= el.clientHeight + 1 && el.scrollWidth <= el.clientWidth + 1);

  // Wide screens, first choice: everything on one page (the scenes on top, the rest side by side under them), or the
  // scenes and the saved presets on one page and the hardware (its three panels next to each other) on another.
  // Returns the pages, or null when neither fits
  const layoutSideBySide = (view) => {
    const { scenes, hardware, presets } = view;
    const panels = hardware.map((part) => part.panel);
    const stack = (...nodes) => { const el = element('t4s-stack'); el.append(...nodes); return el; };
    const attempts = [
      () => { const row = element('t4s-row'); row.append(...panels, presets.panel); return [[stack(scenes.panel, row)]]; },
      () => [[stack(scenes.panel, presets.panel)], panels],
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

  // How many lines of a panel (rows of cards, rows of hardware tiles) fit on a page, measured with the panel alone on it
  const linesOf = (view, part) => {
    const [page] = setPages(view, [[part.panel]]);
    sizeTiles(part);

    const style = getComputedStyle(page);
    const gap = px(getComputedStyle(part.body).rowGap);
    const room = page.clientHeight - px(style.paddingTop) - px(style.paddingBottom) - (part.panel.offsetHeight - part.body.offsetHeight);
    return Math.max(1, Math.floor((room + gap) / (part.items[0].offsetHeight + gap)));
  };

  // Splits a panel into as many panels as it takes, the same size each; returns them (the first is the panel itself).
  // capacity: how many items a page holds, perLine: how many of them make a line (the pages are cut between lines)
  const paginate = (part, capacity, perLine) => {
    const count = Math.ceil(part.items.length / capacity);
    const size = Math.ceil(Math.ceil(part.items.length / count) / perLine) * perLine;
    const panels = [part.panel];

    for (let i = 1; i < count; i++) {
      const copy = part.panel.cloneNode(false);
      const body = part.body.cloneNode(false);
      copy.append(part.panel.querySelector('.t4s-panel-head').cloneNode(true), body);
      body.append(...part.items.slice(i * size, (i + 1) * size));
      panels.push(copy);
    }

    if (count > 1) {
      panels.forEach((panel, i) => { panel.querySelector('.t4s-range').textContent = ` ${i + 1}/${count}`; });
    }
    return panels;
  };

  // Last resort and the way of the small screens: each panel on its own page(s), as many as the height asks for
  const layoutOneByOne = (view) => {
    const { scenes, hardware, presets } = view;
    gather(view);

    const scenePanels = paginate(scenes, linesOf(view, scenes) * scenes.columns, scenes.columns);
    // Each purpose on a page of its own (more than one if the tiles do not fit)
    const tilePanels = hardware.flatMap((part) => {
      const lines = linesOf(view, part);
      return paginate(part, lines * part.columns, part.columns);
    });

    return setPages(view, [...scenePanels, ...tilePanels, presets.panel].map((panel) => [panel]));
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
    const dots = root.querySelector('.t4s-dots');

    view.page = clamp(Math.round(pager.scrollLeft / (pager.clientWidth || 1)), 0, Math.max(0, count - 1));

    if (dots.children.length !== count) {
      dots.innerHTML = range(0, count - 1).map((i) => `<span class="t4s-dot" data-page="${i}"></span>`).join('');
    }
    Array.from(dots.children).forEach((dot, i) => dot.classList.toggle('is-current', i === view.page));

    root.querySelector('.t4s-prev').disabled = view.page <= 0;
    root.querySelector('.t4s-next').disabled = view.page >= count - 1;
    setText(root.querySelector('.t4s-page-status'), `Page ${view.page + 1} of ${count}`);
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

    // Not while a name is being typed (the on-screen keyboard resizes the window; moving the field would close it)
    const focused = document.activeElement;
    if (focused && root.contains(focused) && focused.matches('input[type="text"]')) {
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

    (width >= WIDE_PX && layoutSideBySide(view)) || layoutOneByOne(view);

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
  // Drawing the state on both the icon (pedalboard) and the settings page. The elements are looked up once, on
  // 'start' (cacheElements), and draw() only touches the DOM: it runs on every change the host sends

  const cacheElements = (event) => {
    const all = (root, selector) => root.find(selector).toArray();
    const preset = (el) => Number(el.getAttribute('data-preset'));
    const output = (el) => Number(el.getAttribute('data-output'));
    const withPreset = (root, selector) => all(root, selector).map((el) => ({ el, preset: preset(el) }));
    const withOutput = (root, selector) => all(root, selector).map((el) => ({ el, preset: preset(el), output: output(el) }));

    event.data.el = {
      // Icon
      root: all(event.icon, '.tetr4-switch').concat(all(event.settings, '.t4s')),
      units: withPreset(event.icon, '.t4-unit'),
      foots: withPreset(event.icon, '.t4-foot'),
      names: withPreset(event.icon, '.t4-name'),
      dips: withOutput(event.icon, '.t4-dip'),
      // Settings page
      nowNumbers: all(event.settings, '.t4s-now-num'),
      nowLabels: all(event.settings, '.t4s-now-label'),
      cards: withPreset(event.settings, '.t4s-card'),
      pads: withPreset(event.settings, '.t4s-pad'),
      labels: withPreset(event.settings, '.t4s-label'),
      cells: withOutput(event.settings, '.t4s-dip'),
      // The hardware tiles of the preset buttons are named after the presets
      assignTitles: withPreset(event.settings, '.t4s-assign-title'),
    };
  };

  const toggle = (el, className, on, aria) => {
    el.classList.toggle(className, on);
    if (aria) {
      el.setAttribute(aria, on ? 'true' : 'false');
    }
  };

  const setText = (el, text) => {
    if (el.textContent !== text) {
      el.textContent = text;
    }
  };

  // Don't fight the user while they're still typing on this field
  const setInput = (el, value) => {
    if (el !== document.activeElement && el.value !== value) {
      el.value = value;
    }
  };

  const draw = (event) => {
    const state = event.data.state;
    const el = event.data.el;
    const active = state.active;

    el.root.forEach((root) => root.setAttribute('data-active', active));
    el.nowNumbers.forEach((number) => setText(number, String(active)));
    el.nowLabels.forEach((label) => setText(label, labelOf(state, active)));

    // Icon: the banks of switches, the footswitches and the names
    el.units.forEach((unit) => toggle(unit.el, 'is-active', unit.preset === active));
    el.foots.forEach((foot) => toggle(foot.el, 'is-active', foot.preset === active, 'aria-pressed'));
    el.names.forEach((name) => setInput(name.el, labelOf(state, name.preset)));
    el.dips.forEach((dip) => toggle(dip.el, 'is-on', !!state.dips[dip.preset - 1][dip.output - 1], 'aria-checked'));

    // Settings page
    el.cards.forEach((card) => toggle(card.el, 'is-active', card.preset === active));
    el.pads.forEach((pad) => toggle(pad.el, 'is-active', pad.preset === active, 'aria-pressed'));
    el.labels.forEach((label) => setInput(label.el, labelOf(state, label.preset)));
    el.cells.forEach((cell) => toggle(cell.el, 'is-on', !!state.dips[cell.preset - 1][cell.output - 1], 'aria-checked'));
    el.assignTitles.forEach((title) => setText(title.el, labelOf(state, title.preset)));
  };

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  // Returns whether the active preset changed
  const setActive = (event, preset) => {
    const state = event.data.state;

    if (preset === state.active) {
      return false;
    }

    state.active = preset;
    return true;
  };

  // The user pressed a footswitch of the icon or a preset button of the settings page: it is shown at once, without
  // waiting for the host. The host's answer is handled by hostSays().
  // The click goes to the combobox, not to the selector: MOD UI sends nothing when its copy of the port already holds
  // the value, and its copy of a selector only goes back to 0 when the host answers, so a quick click back to the
  // previous preset was lost. The plugin turns the selectors on and off by itself
  const selectPreset = (event, preset) => {
    const state = event.data.state;

    state.inFlight.push({ preset, at: Date.now() });

    // Always sent, even to the preset that looks active: what this page believes may be out of date (the plugin
    // is the one that knows)
    setActive(event, preset);
    draw(event);

    // Even if no answer ever comes (a click MOD UI dropped), the display goes back to where the host is. Not before a
    // click stops being in flight: a slow host is still answering (hostSays() waits less once it does)
    clearTimeout(event.data.settleTimer);
    event.data.settleTimer = setTimeout(() => reconcile(event), IN_FLIGHT_MS);

    funcs.set_port_value('Preset_combobox', preset - 1);
  };

  // The host says the plugin is on this preset:
  //  - the answer to a click: the click was already shown, nothing to do (and the answers to the clicks before it
  //    are not worth showing either: the display is already past them)
  //  - the same preset as the display: nothing to do
  //  - any other preset, with no click waiting for an answer (a footswitch, MIDI, the knob, or the host going
  //    somewhere else than the click): the host has the last word, show it
  //  - any other preset while clicks are still waiting: their answers are coming, let them settle (see reconcile)
  // Returns whether the display changed
  const hostSays = (event, preset) => {
    const state = event.data.state;
    const now = Date.now();

    state.hostPreset = preset;
    state.inFlight = state.inFlight.filter((click) => now - click.at < IN_FLIGHT_MS);

    const answered = state.inFlight.findIndex((click) => click.preset === preset);
    if (answered >= 0) {
      state.inFlight.splice(0, answered + 1);
    }

    clearTimeout(event.data.settleTimer);
    event.data.settleTimer = setTimeout(() => reconcile(event), SETTLE_MS);

    return preset !== state.active && state.inFlight.length === 0 && setActive(event, preset);
  };

  // The host has been quiet for a while: whatever click it never answered is lost, and where it says it is wins
  const reconcile = (event) => {
    const state = event.data.state;

    state.inFlight = [];

    if (state.hostPreset !== state.active) {
      setActive(event, state.hostPreset);
      draw(event);
    }
  };

  const toggleDip = (event, preset, output) => {
    const state = event.data.state;
    const on = !state.dips[preset - 1][output - 1];

    state.dips[preset - 1][output - 1] = on;
    draw(event);

    funcs.set_port_value(dipSymbol(preset, output), on ? 1 : 0);
  };

  const changeLabel = (event, input, preset) => {
    const sanitized = sanitizeLabel(input.val());

    if (input.val() !== sanitized) {
      input.val(sanitized);
    }

    event.data.state.labels[preset - 1] = sanitized;

    // Keep the other places showing this label in sync with every keystroke
    draw(event);

    funcs.patch_set(labelUri(preset), 's', sanitized);
  };

  const openSettings = (event) => {
    // MOD UI adds its own "settings" action to the plugin icon: the one that opens the settings page
    event.icon.find('.mod-actions .mod-settings').first().trigger('click');
  };

  const assignEvents = (event) => {
    const icon = event.icon;
    const settings = event.settings;
    const number = (el, name) => Number($(el).data(name));

    icon.on('click', '.t4-foot', function () { selectPreset(event, number(this, 'preset')); });
    icon.on('click', '.t4-dip', function () { toggleDip(event, number(this, 'preset'), number(this, 'output')); });
    icon.on('click', '.t4-open', function () { openSettings(event); setTimeout(() => scheduleLayout(event), 0); });
    icon.on('input', '.t4-name', function () { changeLabel(event, $(this), number(this, 'preset')); });
    // Leaving the field: show the stored value again (e.g. an empty name goes back to the default)
    icon.on('blur', '.t4-name', function () { draw(event); });

    settings.on('click', '.t4s-pad', function () { selectPreset(event, number(this, 'preset')); });
    settings.on('click', '.t4s-dip', function () { toggleDip(event, number(this, 'preset'), number(this, 'output')); });
    settings.on('input', '.t4s-label', function () { changeLabel(event, $(this), number(this, 'preset')); });

    settings.on('blur', '.t4s-label', function () {
      draw(event);
      // A resize that came while the name was being typed waited for this
      if (event.data.view.pending) {
        scheduleLayout(event);
      }
    });

    // Pages: the buttons of the bar, the dots, and the arrow keys (a swipe is the pager scrolling by itself)
    settings.on('click', '.t4s-prev', function () { stepPage(event, -1); });
    settings.on('click', '.t4s-next', function () { stepPage(event, 1); });
    settings.on('click', '.t4s-dot', function () { goToPage(event.data.view, number(this, 'page'), true); });
    settings.on('keydown', function (e) {
      if (/^(input|textarea|select)$/i.test(e.target.tagName) || e.altKey || e.ctrlKey || e.metaKey) {
        return;
      }
      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        stepPage(event, e.key === 'ArrowLeft' ? -1 : 1);
        e.preventDefault();
      }
    });
    event.data.view.pager.addEventListener('scroll', () => scheduleNavUpdate(event), { passive: true });

    // The page is laid out when it is shown and whenever the window changes (see layoutSettings)
    if (typeof ResizeObserver === 'function') {
      new ResizeObserver(() => scheduleLayout(event)).observe(settings[0]);
    } else {
      window.addEventListener('resize', () => scheduleLayout(event));
    }
  };

  // ---------------------------------------------------------------------------
  // Events coming from the plugin / MOD UI

  // Returns whether what is drawn may have changed
  const handlePort = (event, symbol, value, afterStart) => {
    const state = event.data.state;
    let match;

    if ((match = /^Preset_(\d+)_output_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);
      const output = Number(match[2]);

      if (preset >= 1 && preset <= PRESETS && output >= 1 && output <= OUTPUTS) {
        const on = value == 1;
        const changed = state.dips[preset - 1][output - 1] !== on;
        state.dips[preset - 1][output - 1] = on;
        return changed;
      }

    } else if ((match = /^Preset_selector_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);

      // The plugin keeps the selectors like radio buttons, so the one that turns on is the preset that is playing
      if (preset >= 1 && preset <= PRESETS && value == 1) {
        if (afterStart) {
          return hostSays(event, preset);
        }
        state.active = state.hostPreset = preset;
        return true;
      }

    } else if (symbol === 'Preset_combobox' && !afterStart) {
      // Only when the page starts, and only if no selector is on
      state.comboboxPreset = Math.round(Number(value)) + 1;
    }

    return false;
  };

  const LABEL_URI_PREFIX = `${URI}#preset_label_`;

  const handleLabel = (event, uri, value) => {
    if (typeof uri !== 'string' || !uri.startsWith(LABEL_URI_PREFIX)) {
      return false;
    }

    const preset = Number(uri.slice(LABEL_URI_PREFIX.length));

    if (!(preset >= 1 && preset <= PRESETS) || event.data.state.labels[preset - 1] === String(value)) {
      return false;
    }

    event.data.state.labels[preset - 1] = String(value);
    return true;
  };

  if (event.type === 'start') {
    event.data.state = newState();

    buildSettings(event.settings);
    cacheElements(event);
    event.data.view = findPanels(event.settings.find('.t4s')[0]);
    assignEvents(event);

    event.data.state.active = 0;
    for (const port of event.ports) {
      handlePort(event, port.symbol, port.value, false);
    }
    if (!event.data.state.active) {
      const preset = event.data.state.comboboxPreset;
      event.data.state.active = event.data.state.hostPreset = preset >= 1 && preset <= PRESETS ? preset : 1;
    }

    // Labels come from the plugin: ask for them (the reply arrives as a 'change' event with a uri)
    range(1, PRESETS).forEach((preset) => funcs.patch_get(labelUri(preset)));

    draw(event);

  } else if (event.data.state) {
    // Reply to funcs.patch_get(uri) ('change') or a patch:Set sent by the plugin ('patch_set'), or a port
    const changed = event.uri
      ? handleLabel(event, event.uri, event.value)
      : event.type === 'change' && handlePort(event, event.symbol, event.value, true);

    if (changed) {
      draw(event);
    }
  }
}
