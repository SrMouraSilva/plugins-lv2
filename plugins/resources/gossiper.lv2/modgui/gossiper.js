function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.
  const FOOTSWITCHES = 7;
  const POTS = 4;
  const URI = 'http://srmourasilva.github.io/plugins/gossiper';
  const LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;
  const MAX_VOLTS = 10;
  const FLASH_MS = 1100;

  const labelUri = (n) => `${URI}#footswitch_label_${n}`;
  const range = (from, to) => Array.from({ length: to - from + 1 }, (_, i) => from + i);
  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  // ---------------------------------------------------------------------------
  // State: { on: [footswitch] => bool, labels: [footswitch] => string, pot: [pot] => 0..1,
  //          min / max: [pot] => volts, last: the control used last: { kind: 'fs' | 'pot', n } or null }

  const newState = () => ({
    on: range(1, FOOTSWITCHES).map(() => false),
    labels: range(1, FOOTSWITCHES).map((n) => `Footswitch ${n}`),
    pot: range(1, POTS).map(() => 0),
    min: range(1, POTS).map(() => 0),
    max: range(1, POTS).map(() => MAX_VOLTS),
    last: null,
  });

  const labelOf = (state, n) => state.labels[n - 1] || `Footswitch ${n}`;
  const percent = (state, n) => Math.round(clamp(state.pot[n - 1], 0, 1) * 100);
  const volts = (value) => `${Number(value).toFixed(2)}`;

  // What the display (on the pedal) and the pill (on the settings page) say about the control used last
  const summarize = (state) => {
    const last = state.last;

    if (!last) {
      return { kind: 'idle', n: 1, badge: '–', caption: 'Last control used', what: 'Nothing yet', on: false, text: '', fill: 0, pill: 'Nothing yet' };
    }

    if (last.kind === 'fs') {
      const on = state.on[last.n - 1];
      const what = labelOf(state, last.n);
      return { kind: 'fs', n: last.n, badge: String(last.n), caption: `Footswitch ${last.n}`, what, on, text: on ? 'ON' : 'OFF', fill: 0, pill: `${what} · ${on ? 'ON' : 'OFF'}` };
    }

    const pct = percent(state, last.n);
    return { kind: 'pot', n: last.n, badge: `P${last.n}`, caption: 'Potentiometer', what: `Pot ${last.n}`, on: false, text: `${pct}%`, fill: pct, pill: `Pot ${last.n} · ${pct}%` };
  };

  // ---------------------------------------------------------------------------
  // Drawing the state on both the icon (pedalboard) and the settings page. The elements are looked up once, on
  // 'start' (cacheElements), and draw() only touches the DOM: it runs on every change the host sends

  const cacheElements = (event) => {
    const all = (root, selector) => root.find(selector).toArray();
    const n = (el) => Number(el.getAttribute('data-n'));
    const within = (el, selector) => el.querySelector(selector);

    event.data.el = {
      // Icon
      display: all(event.icon, '.gp-display'),
      fs: all(event.icon, '.gp-fs').map((el) => ({ el, n: n(el) })),
      labels: all(event.icon, '.gp-label').concat(all(event.settings, '.gps-label')).map((el) => ({ el, n: n(el) })),
      pots: all(event.icon, '.gp-pot').map((el) => ({
        el, n: n(el), meter: within(el, '.gp-meter i'), value: within(el, '.gp-pot-value'), range: within(el, '.gp-pot-range'),
      })),
      // Settings page
      root: all(event.settings, '.gps'),
      now: all(event.settings, '.gps-now'),
      rows: all(event.settings, '.gps-row').map((el) => ({ el, n: n(el), pot: el.classList.contains('gps-pot') })),
      potRows: all(event.settings, '.gps-pot').map((el) => ({
        el, n: n(el), meter: within(el, '.gps-meter i'), percent: within(el, '.gps-pot-percent'),
      })),
      volts: all(event.settings, '.gps-volts').map((el) => ({ el, symbol: el.getAttribute('data-port') })),
    };
  };

  const toggle = (el, className, on) => el.classList.toggle(className, on);

  const setText = (el, text) => {
    if (el && el.textContent !== text) {
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
    const last = state.last;
    const now = summarize(state);

    // The display of the pedal: it keeps saying what was used last
    el.display.forEach((display) => {
      display.setAttribute('data-kind', now.kind);
      display.setAttribute('data-n', now.n);
      toggle(display, 'is-idle', now.kind === 'idle');
      toggle(display, 'is-on', now.on);
      setText(display.querySelector('.gp-badge'), now.badge);
      setText(display.querySelector('.gp-caption'), now.caption);
      setText(display.querySelector('.gp-what'), now.what);
      setText(display.querySelector('.gp-state-text'), now.text);
      display.querySelector('.gp-state-meter i').style.width = `${now.fill}%`;
    });

    // The footswitches and the knobs, the one used last marked
    el.fs.forEach((fs) => {
      toggle(fs.el, 'is-on', state.on[fs.n - 1]);
      toggle(fs.el, 'is-last', !!last && last.kind === 'fs' && last.n === fs.n);
    });
    el.pots.forEach((pot) => {
      const pct = percent(state, pot.n);
      pot.meter.style.width = `${pct}%`;
      setText(pot.value, `${pct}%`);
      setText(pot.range, `${volts(state.min[pot.n - 1])} - ${volts(state.max[pot.n - 1])} V`);
      toggle(pot.el, 'is-last', !!last && last.kind === 'pot' && last.n === pot.n);
    });
    el.labels.forEach((label) => setInput(label.el, labelOf(state, label.n)));

    // Settings page
    el.now.forEach((pill) => {
      pill.setAttribute('data-kind', now.kind);
      pill.setAttribute('data-n', now.n);
      toggle(pill, 'is-idle', now.kind === 'idle');
      setText(pill.querySelector('.gps-now-badge'), now.badge);
      setText(pill.querySelector('.gps-now-label'), now.pill);
    });
    el.rows.forEach((row) => {
      const kind = row.pot ? 'pot' : 'fs';
      toggle(row.el, 'is-last', !!last && last.kind === kind && last.n === row.n);
      if (!row.pot) {
        toggle(row.el, 'is-on', state.on[row.n - 1]);
      }
    });
    el.potRows.forEach((row) => {
      const pct = percent(state, row.n);
      row.meter.style.width = `${pct}%`;
      setText(row.percent, `${pct}%`);
    });
    el.volts.forEach((field) => {
      const [kind, n] = field.symbol.split('_');
      setInput(field.el, String(Math.round(state[kind === 'minimum' ? 'min' : 'max'][Number(n) - 1] * 100) / 100));
    });
  };

  // The display lights up for a moment when a control is used, so a glance at the pedal says "something just happened"
  // (devices such as the MOD Duo have no screen for plugin pop-ups, so this is the only way to see it)
  const flash = (event) => {
    const displays = $(event.data.el.display);

    displays.removeClass('is-changed');
    void displays[0] && displays[0].offsetWidth;
    displays.addClass('is-changed');

    clearTimeout(event.data.flashTimer);
    event.data.flashTimer = setTimeout(() => displays.removeClass('is-changed'), FLASH_MS);
  };

  // A footswitch always lights it up; a knob only when it was another control used last (a sweep is one event)
  const used = (event, kind, n) => {
    const state = event.data.state;
    const same = !!state.last && state.last.kind === kind && state.last.n === n;

    state.last = { kind, n };
    if (kind === 'fs' || !same) {
      flash(event);
    }
  };

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  const changeLabel = (event, input, n) => {
    const sanitized = sanitizeLabel(input.val());

    if (input.val() !== sanitized) {
      input.val(sanitized);
    }

    event.data.state.labels[n - 1] = sanitized;

    // Keep the other places showing this name in sync with every keystroke
    draw(event);

    funcs.patch_set(labelUri(n), 's', sanitized);
  };

  const changeVolts = (event, input, symbol) => {
    const [kind, n] = symbol.split('_');
    const value = parseFloat(String(input.val()).replace(',', '.'));

    // A half-typed number is left alone until it is one
    if (!isFinite(value)) {
      return;
    }

    event.data.state[kind === 'minimum' ? 'min' : 'max'][Number(n) - 1] = clamp(value, 0, MAX_VOLTS);
    draw(event);

    funcs.set_port_value(symbol, clamp(value, 0, MAX_VOLTS));
  };

  const openSettings = (event) => {
    // MOD UI adds its own "settings" action to the plugin icon: the one that opens the settings page
    event.icon.find('.mod-actions .mod-settings').first().trigger('click');
  };

  // ---------------------------------------------------------------------------
  // The settings page never scrolls vertically: its panels (footswitches, potentiometers, pop-up slots, saved presets)
  // are spread over horizontal pages, as few as the screen allows, one swipe (or one of the buttons of the top bar)
  // apart. See docs/custom-settings-pages.md.
  //
  // What fits is measured here instead of left to @media queries. MOD UI's page has no <meta name="viewport"> and its
  // body is at least 1024px wide, so a phone lays it out in about 980px and shrinks it to the screen: a query on
  // the viewport never sees a phone. The script measures the box the page is drawn in (fitToScreen makes that box
  // the size of the real screen) and says what it found with data-w / data-h on the root, for the stylesheet.

  const WIDE_PX = 960;           // from this width, two panels can sit side by side
  const MEDIUM_PX = 700;
  const SHORT_PX = 600;          // below this height, the descriptions go and the rows get tighter
  const TILE_MIN_PX = 200;       // the pop-up slot tiles
  const TILE_MAX_COLUMNS = 4;

  const px = (value) => parseFloat(value) || 0;
  const element = (className) => {
    const el = document.createElement('div');
    el.className = className;
    return el;
  };

  // The panels, the lists they hold and what moves them: found once, on 'start'
  const findPanels = (root) => {
    const part = (name, bodySelector, itemsSelector) => {
      const panel = root.querySelector(`[data-panel="${name}"]`);
      const body = bodySelector && panel.querySelector(bodySelector);
      return { panel, body, items: body ? Array.from(body.querySelectorAll(itemsSelector)) : [], columns: 1 };
    };

    return {
      root,
      pager: root.querySelector('.gps-pager'),
      stock: root.querySelector('.gps-stock'),
      switches: part('switches', '.gps-list', '.gps-row'),
      pots: part('pots', '.gps-list', '.gps-row'),
      notify: part('notify', '.gps-tiles', 'li'),
      presets: part('presets'),
      page: 0,
      key: '',
      pending: false,
    };
  };

  // MOD UI's page has no viewport meta tag, so a phone or a tablet shows it as a ~980px page shrunk to the screen.
  // This draws the page in the size of the screen instead: its box is made as many CSS pixels wide as the screen
  // has, and scaled up to cover the window. Only touch screens that really are shrunk are touched: a desktop
  // browser (zoomed in or out) is left alone. The box is measured afterwards (clientWidth)
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

  const lists = (view) => [view.switches, view.pots, view.notify];

  // Everything back in the stock, in the order it was written, and the pages (with their copies) thrown away
  const gather = (view) => {
    view.stock.append(...lists(view).map((part) => part.panel), view.presets.panel);
    lists(view).forEach((part) => part.body.append(...part.items));
    view.pager.textContent = '';
    view.root.querySelectorAll('.gps-range-label').forEach((label) => { label.textContent = ''; });
  };

  // A page is a list of what is on it side by side: a panel, or a list of panels, which makes a column
  const setPages = (view, pages) => {
    view.pager.textContent = '';
    return pages.map((group) => {
      const page = element('gps-page');
      page.append(...group.map((item) => {
        if (!Array.isArray(item)) {
          return item.panel;
        }
        const col = element('gps-col');
        col.append(...item.map((part) => part.panel));
        return col;
      }));
      view.pager.append(page);
      return page;
    });
  };

  // The columns of the tiles of a panel, from its width (never more columns than tiles)
  const sizeTiles = (part) => {
    const gap = px(getComputedStyle(part.body).columnGap);

    part.columns = clamp(Math.floor((part.body.clientWidth + gap) / (TILE_MIN_PX + gap)), 1, Math.min(TILE_MAX_COLUMNS, part.items.length));
    part.body.style.setProperty('--gps-cols', part.columns);
  };

  const fits = (page) => [page, ...page.querySelectorAll('.gps-panel, .gps-col')]
    .every((el) => el.scrollHeight <= el.clientHeight + 1 && el.scrollWidth <= el.clientWidth + 1);

  // Wide screens, first choice: everything on as few pages as possible, panels next to each other. Returns the pages,
  // or null when none of the arrangements fits
  const layoutSideBySide = (view) => {
    const { switches, pots, notify, presets } = view;
    const attempts = [
      [[switches, [pots, notify]], [presets]],
      [[switches, pots], [notify, presets]],
    ];

    for (const attempt of attempts) {
      gather(view);
      const pages = setPages(view, attempt);
      sizeTiles(notify);
      if (pages.every(fits)) {
        return pages;
      }
    }
    return null;
  };

  // How many lines of a panel (rows, rows of tiles) fit on a page, measured with the panel alone on it
  const linesOf = (view, part) => {
    const [page] = setPages(view, [[part]]);
    if (part === view.notify) {
      sizeTiles(part);
    }

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
      copy.append(part.panel.querySelector('.gps-panel-head').cloneNode(true), body);
      body.append(...part.items.slice(i * size, (i + 1) * size));
      panels.push(copy);
    }

    if (count > 1) {
      panels.forEach((panel, i) => { panel.querySelector('.gps-range-label').textContent = ` ${i + 1}/${count}`; });
    }
    return panels;
  };

  // Last resort and the way of the small screens: each panel on its own page(s), as many as the height asks for
  const layoutOneByOne = (view) => {
    gather(view);

    const panels = lists(view).flatMap((part) => {
      const lines = linesOf(view, part);
      return paginate(part, lines * part.columns, part.columns);
    });

    return setPages(view, [...panels, view.presets.panel].map((panel) => [{ panel }]));
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
    const dots = root.querySelector('.gps-dots');

    view.page = clamp(Math.round(pager.scrollLeft / (pager.clientWidth || 1)), 0, Math.max(0, count - 1));

    if (dots.children.length !== count) {
      dots.innerHTML = range(0, count - 1).map((i) => `<span class="gps-dot" data-page="${i}"></span>`).join('');
    }
    Array.from(dots.children).forEach((dot, i) => dot.classList.toggle('is-current', i === view.page));

    root.querySelector('.gps-prev').disabled = view.page <= 0;
    root.querySelector('.gps-next').disabled = view.page >= count - 1;
    setText(root.querySelector('.gps-page-status'), `Page ${view.page + 1} of ${count}`);
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

    // Not while a name or a number is being typed (the on-screen keyboard resizes the window; moving the field
    // would close it)
    const focused = document.activeElement;
    if (focused && root.contains(focused) && focused.matches('input[type="text"], input[type="number"]')) {
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
  // Events

  const assignEvents = (event) => {
    const icon = event.icon;
    const settings = event.settings;
    const view = event.data.view;

    icon.on('click', '.gp-open', function () { openSettings(event); setTimeout(() => scheduleLayout(event), 0); });
    icon.on('input', '.gp-label', function () { changeLabel(event, $(this), Number($(this).data('n'))); });
    icon.on('blur', '.gp-label', function () { draw(event); });

    settings.on('input', '.gps-label', function () { changeLabel(event, $(this), Number($(this).data('n'))); });
    settings.on('input', '.gps-volts', function () { changeVolts(event, $(this), $(this).attr('data-port')); });

    // Leaving the field: show the stored value again (e.g. an empty name goes back to the default)
    settings.on('blur', '.gps-label, .gps-volts', function () {
      draw(event);
      // A resize that came while it was being typed waited for this
      if (view.pending) {
        scheduleLayout(event);
      }
    });

    // Pages: the buttons of the bar, the dots, and the arrow keys (a swipe is the pager scrolling by itself)
    settings.on('click', '.gps-prev', function () { stepPage(event, -1); });
    settings.on('click', '.gps-next', function () { stepPage(event, 1); });
    settings.on('click', '.gps-dot', function () { goToPage(view, Number($(this).data('page')), true); });
    settings.on('keydown', function (e) {
      if (/^(input|textarea|select)$/i.test(e.target.tagName) || e.altKey || e.ctrlKey || e.metaKey) {
        return;
      }
      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        stepPage(event, e.key === 'ArrowLeft' ? -1 : 1);
        e.preventDefault();
      }
    });
    view.pager.addEventListener('scroll', () => scheduleNavUpdate(event), { passive: true });

    // The page is laid out when it is shown and whenever the window changes (see layoutSettings)
    if (typeof ResizeObserver === 'function') {
      new ResizeObserver(() => scheduleLayout(event)).observe(settings[0]);
    } else {
      window.addEventListener('resize', () => scheduleLayout(event));
    }
  };

  // Returns whether what is drawn may have changed. announceChange: false while the page starts (what a control
  // holds then is not something that was just used)
  const handlePort = (event, symbol, value, announceChange) => {
    const state = event.data.state;
    let match;

    if ((match = /^Footswitch_(\d+)$/.exec(symbol || ''))) {
      const n = Number(match[1]);

      if (n >= 1 && n <= FOOTSWITCHES) {
        const on = value == 1;
        const changed = state.on[n - 1] !== on;
        state.on[n - 1] = on;
        if (announceChange && changed) {
          used(event, 'fs', n);
        }
        return changed;
      }

    } else if ((match = /^Potentiometer_(\d+)$/.exec(symbol || ''))) {
      const n = Number(match[1]);

      if (n >= 1 && n <= POTS) {
        const changed = state.pot[n - 1] !== Number(value);
        state.pot[n - 1] = Number(value);
        if (announceChange && changed) {
          used(event, 'pot', n);
        }
        return changed;
      }

    } else if ((match = /^(minimum|maximum)_(\d+)$/.exec(symbol || ''))) {
      const n = Number(match[2]);

      if (n >= 1 && n <= POTS) {
        state[match[1] === 'minimum' ? 'min' : 'max'][n - 1] = Number(value);
        return true;
      }
    }

    return false;
  };

  const LABEL_URI_PREFIX = `${URI}#footswitch_label_`;

  const handleLabel = (event, uri, value) => {
    if (typeof uri !== 'string' || !uri.startsWith(LABEL_URI_PREFIX)) {
      return false;
    }

    const n = Number(uri.slice(LABEL_URI_PREFIX.length));

    if (!(n >= 1 && n <= FOOTSWITCHES) || event.data.state.labels[n - 1] === String(value)) {
      return false;
    }

    event.data.state.labels[n - 1] = String(value);
    return true;
  };

  if (event.type === 'start') {
    event.data.state = newState();
    event.data.view = findPanels(event.settings.find('.gps')[0]);

    cacheElements(event);
    assignEvents(event);

    for (const port of event.ports) {
      handlePort(event, port.symbol, port.value, false);
    }

    // Labels come from the plugin: ask for them (the reply arrives as a 'change' event with a uri)
    range(1, FOOTSWITCHES).forEach((n) => funcs.patch_get(labelUri(n)));

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
