function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.
  const PRESETS = 7;        // one footswitch each; as many as the colours of the HMI LEDs
  const OUTPUTS = 8;        // CV outputs, and so dip switches per preset
  const URI = 'http://srmourasilva.github.io/plugins/hept7-switch';
  const LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;
  const TOAST_MS = 1700;
  // The host answers a click some time later, and several clicks can be in flight: the answers to the previous
  // ones must not flicker the display. A click is "in flight" until the host answers it, or this long passes with
  // the host quiet (MOD UI sends nothing when a click does not change the port, so some are never answered)
  const SETTLE_MS = 800;
  const IN_FLIGHT_MS = 2000;

  const labelUri = (preset) => `${URI}#preset_label_${preset}`;
  const dipSymbol = (preset, output) => `Preset_${preset}_output_${output}`;
  const cycleSymbol = (preset) => `Preset_${preset}_in_cycle`;
  const range = (from, to) => Array.from({ length: to - from + 1 }, (_, i) => from + i);

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  // ---------------------------------------------------------------------------
  // State: { active: 1..7, editing: 1..7, dips: [preset][output] => bool, labels: [preset] => string,
  //          inCycle: [preset] => bool (one of the presets "Current preset" goes through),
  //          inFlight: [{preset, at}] the clicks the host has not answered yet, hostPreset: what the host last said,
  //          selectorCopy: [preset] => MOD UI's copy of each Preset_selector port (see selectPreset) }

  const newState = () => ({
    active: 1,
    editing: 1,
    // Same defaults as the TTL: preset N turns on output N
    dips: range(1, PRESETS).map((p) => range(1, OUTPUTS).map((o) => p === o)),
    labels: range(1, PRESETS).map((p) => `Preset ${p}`),
    inCycle: range(1, PRESETS).map(() => true),
    inFlight: [],
    hostPreset: 1,
    selectorCopy: range(1, PRESETS).map((p) => p === 1 ? 1 : 0),
  });

  const labelOf = (state, preset) => state.labels[preset - 1] || `Preset ${preset}`;

  // ---------------------------------------------------------------------------
  // Building the settings page (the pedalboard icon is static HTML)

  const rowHtml = (preset) => {
    const cells = range(1, OUTPUTS).map((output) =>
      `<button type="button" role="switch" aria-checked="false" class="h7s-cell" data-preset="${preset}" data-output="${output}" ` +
      `aria-label="Preset ${preset}, CV output ${output}" title="Preset ${preset} - Output ${output}"></button>`
    ).join('');

    // Set apart from the CV outputs: whether "Current preset" goes through this preset
    const cycle = `<button type="button" role="switch" aria-checked="true" class="h7s-cell h7s-cell-cycle" data-preset="${preset}" ` +
      `aria-label="Preset ${preset} is in the cycle of Current preset" title="Preset ${preset} - In the cycle of Current preset (footswitch, B+C, knob)"></button>`;

    return `<div class="h7s-row p${preset}" data-preset="${preset}">` +
      `<button type="button" class="h7s-rowpad" data-preset="${preset}" aria-pressed="false" title="Make preset ${preset} the active preset"><b>${preset}</b></button>` +
      `<input type="text" class="h7s-label" data-preset="${preset}" maxlength="${LABEL_MAX_LENGTH}" autocomplete="off" autocapitalize="off" spellcheck="false" ` +
      `aria-label="Name of preset ${preset}" value="Preset ${preset}" />` +
      `<div class="h7s-cells">${cells}${cycle}</div>` +
      // Narrow screens edit one preset at a time, which may not be the one playing
      `<button type="button" class="h7s-activate" data-preset="${preset}">Switch to preset ${preset}</button>` +
      `</div>`;
  };

  const buildSettings = (settings) => {
    settings.find('.h7s-chips').html(range(1, PRESETS).map((p) =>
      `<button type="button" class="h7s-chip p${p}" data-preset="${p}" aria-pressed="false" title="Edit preset ${p}"><b>${p}</b></button>`
    ).join(''));

    const head = '<div class="h7s-row h7s-head" aria-hidden="true"><span class="h7s-corner">Preset</span><span class="h7s-spacer"></span>' +
      '<div class="h7s-cells">' + range(1, OUTPUTS).map((o) => `<span class="h7s-col">${o}</span>`).join('') +
      '<span class="h7s-col h7s-col-cycle" title="In the cycle of Current preset">Cycle</span></div></div>';

    settings.find('.h7s-matrix').html(head + range(1, PRESETS).map(rowHtml).join(''));
  };

  // ---------------------------------------------------------------------------
  // Drawing the state on both the icon (pedalboard) and the settings page. The elements are looked up once, on
  // 'start' (cacheElements), and draw() only touches the DOM: it runs on every change the host sends

  const cacheElements = (event) => {
    const all = (root, selector) => root.find(selector).toArray();
    const preset = (el) => Number(el.getAttribute('data-preset'));
    const output = (el) => Number(el.getAttribute('data-output'));

    event.data.el = {
      // Icon
      pads: all(event.icon, '.h7-pad').map((el) => ({ el, preset: preset(el) })),
      display: all(event.icon, '.h7-display'),
      numbers: all(event.icon, '.h7-num, .h7-caption-num').concat(all(event.settings, '.h7s-now-num')),
      iconLabels: all(event.icon, '.h7-label'),
      outs: all(event.icon, '.h7-out').map((el) => ({ el, output: output(el) })),
      toastNumbers: all(event.icon, '.h7-toast-num'),
      toastLabels: all(event.icon, '.h7-toast-label'),
      // Settings page
      root: all(event.settings, '.h7s'),
      nowLabels: all(event.settings, '.h7s-now-label'),
      rowpads: all(event.settings, '.h7s-rowpad').map((el) => ({ el, preset: preset(el) })),
      chips: all(event.settings, '.h7s-chip').map((el) => ({ el, preset: preset(el) })),
      rows: all(event.settings, '.h7s-row[data-preset]').map((el) => ({ el, preset: preset(el) })),
      cells: all(event.settings, '.h7s-cell[data-output]').map((el) => ({ el, preset: preset(el), output: output(el) })),
      cycleCells: all(event.settings, '.h7s-cell-cycle').map((el) => ({ el, preset: preset(el) })),
      labels: all(event.settings, '.h7s-label').map((el) => ({ el, preset: preset(el) })),
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
    const activeLabel = labelOf(state, active);
    const activeDips = state.dips[active - 1];

    // Icon: the mini display and the buttons
    el.pads.forEach((pad) => {
      toggle(pad.el, 'is-active', pad.preset === active, 'aria-pressed');
      toggle(pad.el, 'is-out-of-cycle', !state.inCycle[pad.preset - 1]);
    });
    el.display.forEach((display) => display.setAttribute('data-preset', active));
    el.numbers.forEach((number) => setText(number, String(active)));
    el.iconLabels.forEach((label) => {
      label.setAttribute('data-preset', active);
      setInput(label, activeLabel);
    });
    el.outs.forEach((out) => toggle(out.el, 'is-on', !!activeDips[out.output - 1], 'aria-checked'));

    // Settings page
    el.root.forEach((root) => {
      root.setAttribute('data-active', active);
      root.setAttribute('data-editing', state.editing);
    });
    el.nowLabels.forEach((label) => setText(label, activeLabel));
    el.rowpads.forEach((pad) => toggle(pad.el, 'is-active', pad.preset === active, 'aria-pressed'));
    el.chips.forEach((chip) => {
      toggle(chip.el, 'is-active', chip.preset === active);
      toggle(chip.el, 'is-editing', chip.preset === state.editing, 'aria-pressed');
    });
    el.rows.forEach((row) => {
      toggle(row.el, 'is-active', row.preset === active);
      toggle(row.el, 'is-editing', row.preset === state.editing);
    });
    el.cells.forEach((cell) => toggle(cell.el, 'is-on', !!state.dips[cell.preset - 1][cell.output - 1], 'aria-checked'));
    el.cycleCells.forEach((cell) => toggle(cell.el, 'is-on', state.inCycle[cell.preset - 1], 'aria-checked'));
    el.labels.forEach((label) => setInput(label.el, labelOf(state, label.preset)));
  };

  // ---------------------------------------------------------------------------
  // The display announces a change of preset: devices such as the MOD Duo have no screen for
  // plugin pop-ups, so this is how the musician sees which preset a footswitch has just selected

  const announce = (event) => {
    const state = event.data.state;
    const el = event.data.el;
    const display = $(el.display);

    el.toastNumbers.forEach((number) => setText(number, String(state.active)));
    el.toastLabels.forEach((label) => setText(label, labelOf(state, state.active)));

    // Restart the animation even if the previous announcement is still on
    display.removeClass('is-changed');
    void display[0] && display[0].offsetWidth;
    display.addClass('is-changed');

    clearTimeout(event.data.toastTimer);
    event.data.toastTimer = setTimeout(() => display.removeClass('is-changed'), TOAST_MS);
  };

  const setActive = (event, preset) => {
    const state = event.data.state;

    if (preset === state.active) {
      return false;
    }

    // The editing row follows the active one, unless the user went to look at another preset
    const followed = state.editing === state.active;

    state.active = preset;
    if (followed) {
      state.editing = preset;
    }

    announce(event);
    return true;
  };

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  // The user pressed a preset button: the display announces it at once, without waiting for the host. The host's
  // answer is handled by hostSays().
  // The click goes to the selector of that preset (not to "Current preset": when some presets are out of its cycle,
  // the plugin takes a move of one step of it for a footswitch going through the cycle). MOD UI sends nothing when its
  // copy of the port already holds the value, and its copy of a selector is only put back to 0 when the host's answer
  // arrives, so a quick click back to a previous preset was lost. This page follows that copy: when it is already on,
  // the selector is turned off and on again, which the plugin always takes as that preset being selected
  const selectPreset = (event, preset) => {
    const state = event.data.state;

    state.inFlight.push({ preset, at: Date.now() });
    state.editing = preset;

    // Always announced and always sent, even to the preset that looks active: what this page believes may be out of
    // date (the plugin is the one that knows)
    if (!setActive(event, preset)) {
      announce(event);
    }
    draw(event);

    // Even if no answer ever comes (a click MOD UI dropped), the display goes back to where the host is. Not before a
    // click stops being in flight: a slow host is still answering (hostSays() waits less once it does)
    clearTimeout(event.data.settleTimer);
    event.data.settleTimer = setTimeout(() => reconcile(event), IN_FLIGHT_MS);

    // The plugin turns the other selectors off by itself
    const symbol = `Preset_selector_${preset}`;
    if (state.selectorCopy[preset - 1] == 1) {
      funcs.set_port_value(symbol, 0);
    }
    funcs.set_port_value(symbol, 1);
    state.selectorCopy[preset - 1] = 1;
  };

  const toggleCycle = (event, preset) => {
    const state = event.data.state;
    const on = !state.inCycle[preset - 1];

    state.inCycle[preset - 1] = on;
    draw(event);

    funcs.set_port_value(cycleSymbol(preset), on ? 1 : 0);
  };

  // The host says the plugin is on this preset:
  //  - the answer to a click: the click was already shown, nothing to do (and the answers to the clicks before it
  //    are not worth showing either: the display is already past them)
  //  - the same preset as the display: nothing to do
  //  - any other preset, with no click waiting for an answer (a footswitch, MIDI, the knob, or the host going
  //    somewhere else than the click): the host has the last word, show it and announce it
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

    icon.on('click', '.h7-pad', function () { selectPreset(event, Number($(this).data('preset'))); });
    // The squares of the display are the CV outputs of the active preset: click one to turn it on or off
    icon.on('click', '.h7-out', function () { toggleDip(event, event.data.state.active, Number($(this).data('output'))); });
    icon.on('click', '.h7-open', function () { openSettings(event); });
    icon.on('input', '.h7-label', function () { changeLabel(event, $(this), event.data.state.active); });
    icon.on('blur', '.h7-label', function () { draw(event); });

    settings.on('click', '.h7s-rowpad, .h7s-activate', function () { selectPreset(event, Number($(this).data('preset'))); });
    settings.on('click', '.h7s-chip', function () {
      event.data.state.editing = Number($(this).data('preset'));
      draw(event);
    });
    settings.on('click', '.h7s-cell[data-output]', function () {
      toggleDip(event, Number($(this).data('preset')), Number($(this).data('output')));
    });
    settings.on('click', '.h7s-cell-cycle', function () { toggleCycle(event, Number($(this).data('preset'))); });
    settings.on('input', '.h7s-label', function () { changeLabel(event, $(this), Number($(this).data('preset'))); });

    // Leaving the field: show the stored value again (e.g. an empty name goes back to the default)
    settings.on('blur', '.h7s-label', function () { draw(event); });
  };

  // ---------------------------------------------------------------------------
  // Events coming from the plugin / MOD UI

  // Returns whether what is drawn may have changed
  const handlePort = (event, symbol, value, announceChange) => {
    const state = event.data.state;
    let match;

    const hostPreset = (preset) => {
      if (announceChange) {
        return hostSays(event, preset);
      }
      state.active = state.hostPreset = preset;
      return true;
    };

    if ((match = /^Preset_(\d+)_output_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);
      const output = Number(match[2]);

      if (preset >= 1 && preset <= PRESETS && output >= 1 && output <= OUTPUTS) {
        const on = value == 1;
        const changed = state.dips[preset - 1][output - 1] !== on;
        state.dips[preset - 1][output - 1] = on;
        return changed;
      }

    } else if ((match = /^Preset_(\d+)_in_cycle$/.exec(symbol || ''))) {
      const preset = Number(match[1]);

      if (preset >= 1 && preset <= PRESETS) {
        const on = value == 1;
        const changed = state.inCycle[preset - 1] !== on;
        state.inCycle[preset - 1] = on;
        return changed;
      }

    } else if ((match = /^Preset_selector_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);

      if (preset >= 1 && preset <= PRESETS) {
        state.selectorCopy[preset - 1] = value == 1 ? 1 : 0;

        // True when it comes, but it does not always come: the host does not echo a selector this page turned on
        if (value == 1) {
          return hostPreset(preset);
        }
      }

    } else if (symbol === 'Active_preset') {
      // The plugin's output: the host reports every change of preset through it, whoever made it. Only after 'start'
      // (at 'start' an output has its default value, not the plugin's)
      const preset = Math.round(Number(value)) + 1;

      if (announceChange && preset >= 1 && preset <= PRESETS) {
        return hostPreset(preset);
      }

    } else if (symbol === 'Preset_combobox' && !announceChange) {
      // Only when the page starts, and only if no selector is on. Afterwards the selectors say which preset is on:
      // with presets out of the cycle, "Current preset" may hold where the device's footswitch is in the cycle
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

    event.data.state.editing = event.data.state.active;
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
