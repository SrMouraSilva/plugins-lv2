function (event, funcs) {
  const PRESET_LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;

  const presetLabelUris = [
    'http://srmourasilva.github.io/plugins/tetr4-switch#preset_label_1',
    'http://srmourasilva.github.io/plugins/tetr4-switch#preset_label_2',
    'http://srmourasilva.github.io/plugins/tetr4-switch#preset_label_3',
    'http://srmourasilva.github.io/plugins/tetr4-switch#preset_label_4',
  ];

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, PRESET_LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  const handleEvent = (symbol, value) => {
    if (symbol && symbol.includes("Preset_") && symbol.includes("_output_")) {
      event.icon.find(`label[data-switch=${symbol}] input`).prop("checked", value == 1);
    }

    if (symbol && symbol.includes("Preset_selector_")) {
      const id = symbol.charAt(symbol.length - 1);
      event.icon.find(`.${symbol}`).prop("checked", value == 1);

      if (value == 1) {
        event.icon.find(`.dip-switch.active`)?.removeClass("active");
        event.icon.find(`.dip-switch.group${id}`).addClass("active");
      }
    }
  }

  const handleLabelChanged = (uri, value) => {
    const input = event.icon.find(`.footswitch-label[mod-parameter-uri="${uri}"]`);

    // Don't fight the user while they're still typing on this field
    if (input.length === 0 || input.is(":focus")) {
      return;
    }

    input.val(value);
  }

  const eventLabelInput = (input) => {
    const uri = input.attr("mod-parameter-uri");
    const sanitized = sanitizeLabel(input.val());

    if (input.val() !== sanitized) {
      input.val(sanitized);
    }

    funcs.patch_set(uri, "s", sanitized);
  }

  const eventFootswitch = (id) => {
    const current = event.icon.find(`input[type=radio][checked]`).prop("value");

    if (id == current) {
      return;
    }

    event.icon.find(`.dip-switch.active`)?.removeClass("active");
    event.icon.find(`.dip-switch.group${id}`).addClass("active");

    event.icon.find(`input[type=radio][checked]`).removeAttr("checked");
    event.icon.find(`input[type=radio][value=${id}]`).attr("checked", "checked");

    funcs.set_port_value(`Preset_selector_${id}`, 1);
    //funcs.set_port_value(`Preset_selector_${current}`, 0);
  }

  const eventSwitch = (group, toggle) => {
    const currentState = event.icon.find(`label[data-switch=Preset_${group}_output_${toggle}] input`).prop("checked");
    
    funcs.set_port_value(`Preset_${group}_output_${toggle}`, currentState ? 1 : 0);
  }

  const assignEvents = () => {
    for (let group=1; group<=4; group++) {
      for (let toggle=1; toggle<=4; toggle++) {
        event.icon.find(`label[data-switch=Preset_${group}_output_${toggle}]`).on("click", () => eventSwitch(group, toggle));
      }
    }

    for (let i=1; i<=4; i++) {
      event.icon.find(`.Preset_selector_${i}`).on("click", () => eventFootswitch(i));
    }

    event.icon.find(`.footswitch-label`).on("input", function () {
      eventLabelInput($(this));
    });
  }

  if (event.type == 'start') {
    assignEvents();

    const ports = event.ports;

    for (let p in ports) {
      handleEvent(ports[p].symbol, ports[p].value);
    }

    presetLabelUris.forEach((uri) => funcs.patch_get(uri));

  } else if (event.type == 'change' && event.uri) {
    // Reply to funcs.patch_get(uri): { type: 'change', uri, value }
    handleLabelChanged(event.uri, event.value);

  } else if (event.type == 'change') {
    handleEvent(event.symbol, event.value);

  } else if (event.type == 'patch_set') {
    handleLabelChanged(event.uri, event.value);
  }
}
