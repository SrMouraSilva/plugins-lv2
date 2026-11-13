function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.
  const TOTAL = 8;
  const URI = 'http://srmourasilva.github.io/plugins/hept7-switch';
  const LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;

  const labelUri = (preset) => `${URI}#preset_label_${preset}`;
  const dipSymbol = (preset, output) => `Preset_${preset}_output_${output}`;
  const range = (from, to) => Array.from({ length: to - from + 1 }, (_, i) => from + i);

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  // ---------------------------------------------------------------------------
  // State: { active: 1..8, editing: 1..8, dips: [preset][output] => bool, labels: [preset] => string }

  const newState = () => ({
    active: 1,
    editing: 1,
    // Same defaults as the TTL: preset N turns on output N
    dips: range(1, TOTAL).map((p) => range(1, TOTAL).map((o) => p === o)),
    labels: range(1, TOTAL).map((p) => `Preset ${p}`),
  });

  const labelOf = (state, preset) => state.labels[preset - 1] || `Preset ${preset}`;

  // ---------------------------------------------------------------------------
  // Building the screens

  const padHtml = (preset, extraClass) =>
    `<button type="button" class="h7s-pad p${preset} ${extraClass || ''}" data-preset="${preset}" aria-pressed="false" title="Preset ${preset}"><span>${preset}</span></button>`;

  const rowHtml = (preset) => {
    const cells = range(1, TOTAL).map((output) =>
      `<button type="button" role="switch" aria-checked="false" class="h7s-cell" data-preset="${preset}" data-output="${output}" ` +
      `aria-label="Preset ${preset}, CV output ${output}" title="Preset ${preset} - Output ${output}"></button>`
    ).join('');

    return `<div class="h7s-row p${preset}" data-preset="${preset}">` +
      `<button type="button" class="h7s-rowpad" data-preset="${preset}" aria-pressed="false" title="Make preset ${preset} the active preset"><i></i><b>${preset}</b></button>` +
      `<input type="text" class="h7s-label" data-preset="${preset}" maxlength="${LABEL_MAX_LENGTH}" autocomplete="off" autocapitalize="off" spellcheck="false" ` +
      `aria-label="Name of preset ${preset}" value="Preset ${preset}" />` +
      `<div class="h7s-cells">${cells}</div>` +
      // Narrow screens edit one preset at a time, which may not be the one playing
      `<button type="button" class="h7s-activate" data-preset="${preset}">Switch to preset ${preset}</button>` +
      `</div>`;
  };

  const buildSettings = (settings) => {
    settings.find('.h7s-rail-left').html(range(1, 4).map((p) => padHtml(p)).join(''));
    settings.find('.h7s-rail-right').html(range(5, 8).map((p) => padHtml(p)).join(''));

    settings.find('.h7s-chips').html(range(1, TOTAL).map((p) =>
      `<button type="button" class="h7s-chip p${p}" data-preset="${p}" aria-pressed="false" title="Edit preset ${p}"><i></i><b>${p}</b></button>`
    ).join(''));

    const head = '<div class="h7s-row h7s-head" aria-hidden="true"><span class="h7s-corner">Preset</span><span class="h7s-spacer"></span>' +
      '<div class="h7s-cells">' + range(1, TOTAL).map((o) => `<span class="h7s-col">${o}</span>`).join('') + '</div></div>';

    settings.find('.h7s-matrix').html(head + range(1, TOTAL).map(rowHtml).join(''));
  };

  // ---------------------------------------------------------------------------
  // Drawing the state on both the icon (pedalboard) and the settings page

  const draw = (event) => {
    const state = event.data.state;
    const icon = event.icon;
    const settings = event.settings;
    const activeLabel = labelOf(state, state.active);

    // Icon
    icon.find('.h7-pad').each(function () {
      const on = Number($(this).data('preset')) === state.active;
      $(this).toggleClass('is-active', on).attr('aria-pressed', on);
    });
    icon.find('.h7-count-num, .h7-now-num').text(state.active);
    icon.find('.h7-now-label').text(activeLabel);
    icon.find('.h7-now').attr('data-preset', state.active);
    icon.find('.h7-out').each(function () {
      $(this).toggleClass('is-on', !!state.dips[state.active - 1][Number($(this).data('output')) - 1]);
    });

    // Settings page
    settings.find('.h7s').attr('data-active', state.active).attr('data-editing', state.editing);
    settings.find('.h7s-now-num').text(state.active);
    settings.find('.h7s-now-label').text(activeLabel);

    settings.find('.h7s-pad, .h7s-rowpad').each(function () {
      const on = Number($(this).data('preset')) === state.active;
      $(this).toggleClass('is-active', on).attr('aria-pressed', on);
    });
    settings.find('.h7s-chip').each(function () {
      const preset = Number($(this).data('preset'));
      $(this).toggleClass('is-active', preset === state.active)
             .toggleClass('is-editing', preset === state.editing)
             .attr('aria-pressed', preset === state.editing);
    });
    settings.find('.h7s-row[data-preset]').each(function () {
      const preset = Number($(this).data('preset'));
      $(this).toggleClass('is-active', preset === state.active)
             .toggleClass('is-editing', preset === state.editing);
    });
    settings.find('.h7s-cell').each(function () {
      const on = !!state.dips[Number($(this).data('preset')) - 1][Number($(this).data('output')) - 1];
      $(this).toggleClass('is-on', on).attr('aria-checked', on);
    });
    settings.find('.h7s-label').each(function () {
      // Don't fight the user while they're still typing on this field
      if (!$(this).is(':focus')) {
        $(this).val(labelOf(state, Number($(this).data('preset'))));
      }
    });
  };

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  const selectPreset = (event, preset) => {
    const state = event.data.state;

    if (preset === state.active) {
      state.editing = preset;
      draw(event);
      return;
    }

    state.active = preset;
    state.editing = preset;
    draw(event);

    // The plugin turns the other selectors off by itself
    funcs.set_port_value(`Preset_selector_${preset}`, 1);
  };

  const toggleDip = (event, preset, output) => {
    const state = event.data.state;
    const on = !state.dips[preset - 1][output - 1];

    state.dips[preset - 1][output - 1] = on;
    draw(event);

    funcs.set_port_value(dipSymbol(preset, output), on ? 1 : 0);
  };

  const changeLabel = (event, input) => {
    const preset = Number(input.data('preset'));
    const sanitized = sanitizeLabel(input.val());

    if (input.val() !== sanitized) {
      input.val(sanitized);
    }

    event.data.state.labels[preset - 1] = sanitized;

    // Only what changed with this keystroke: the label on the icon and in the header
    const label = labelOf(event.data.state, event.data.state.active);
    event.icon.find('.h7-now-label').text(label);
    event.settings.find('.h7s-now-label').text(label);

    funcs.patch_set(labelUri(preset), 's', sanitized);
  };

  const openSettings = (event) => {
    // MOD UI adds its own "settings" action to the plugin icon: the one that opens this page
    event.icon.find('.mod-actions .mod-settings').first().trigger('click');
  };

  const assignEvents = (event) => {
    const icon = event.icon;
    const settings = event.settings;

    icon.on('click', '.h7-pad', function () { selectPreset(event, Number($(this).data('preset'))); });
    icon.on('click', '.h7-open', function () { openSettings(event); });

    settings.on('click', '.h7s-pad, .h7s-rowpad, .h7s-activate', function () { selectPreset(event, Number($(this).data('preset'))); });
    settings.on('click', '.h7s-chip', function () {
      event.data.state.editing = Number($(this).data('preset'));
      draw(event);
    });
    settings.on('click', '.h7s-cell', function () {
      toggleDip(event, Number($(this).data('preset')), Number($(this).data('output')));
    });
    settings.on('input', '.h7s-label', function () { changeLabel(event, $(this)); });

    // Leaving the field: show the stored value again (e.g. an empty name goes back to the default)
    settings.on('blur', '.h7s-label', function () { draw(event); });
  };

  // ---------------------------------------------------------------------------
  // Events coming from the plugin / MOD UI

  const handlePort = (event, symbol, value) => {
    const state = event.data.state;
    let match;

    if ((match = /^Preset_(\d+)_output_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);
      const output = Number(match[2]);

      if (preset >= 1 && preset <= TOTAL && output >= 1 && output <= TOTAL) {
        state.dips[preset - 1][output - 1] = value == 1;
      }

    } else if ((match = /^Preset_selector_(\d+)$/.exec(symbol || ''))) {
      const preset = Number(match[1]);

      if (value == 1 && preset >= 1 && preset <= TOTAL) {
        const followed = state.editing === state.active;
        state.active = preset;
        if (followed) {
          state.editing = preset;
        }
      }

    } else if (symbol === 'Preset_combobox') {
      const preset = Math.round(Number(value)) + 1;

      if (preset >= 1 && preset <= TOTAL) {
        const followed = state.editing === state.active;
        state.active = preset;
        if (followed) {
          state.editing = preset;
        }
      }
    }
  };

  const handleLabel = (event, uri, value) => {
    const match = new RegExp(`^${URI.replace(/[.\/]/g, '\\$&')}#preset_label_(\\d+)$`).exec(uri || '');

    if (!match) {
      return;
    }

    const preset = Number(match[1]);

    if (preset >= 1 && preset <= TOTAL) {
      event.data.state.labels[preset - 1] = String(value);
    }
  };

  if (event.type === 'start') {
    event.data.state = newState();

    buildSettings(event.settings);
    assignEvents(event);

    for (const port of event.ports) {
      handlePort(event, port.symbol, port.value);
    }

    // Labels come from the plugin: ask for them (the reply arrives as a 'change' event with a uri)
    range(1, TOTAL).forEach((preset) => funcs.patch_get(labelUri(preset)));

    event.data.state.editing = event.data.state.active;
    draw(event);

  } else if (event.data.state) {
    if (event.uri) {
      // Reply to funcs.patch_get(uri) ('change') or a patch:Set sent by the plugin ('patch_set')
      handleLabel(event, event.uri, event.value);
    } else if (event.type === 'change') {
      handlePort(event, event.symbol, event.value);
    }

    draw(event);
  }
}
