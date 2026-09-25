function (event, funcs) {
  const FOOTSWITCH_LABEL_MAX_LENGTH = 15;
  const INVALID_LABEL_CHARS = /[^a-zA-Z0-9+\-:_. ]/g;

  const footswitchLabelUris = [
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_1',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_2',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_3',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_4',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_5',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_6',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_7',
  ];

  // Mirrors is_valid_label_char() / sanitize_label() in plugins/src/utils/utils.c
  const sanitizeLabel = (value) => value
    .slice(0, FOOTSWITCH_LABEL_MAX_LENGTH)
    .replace(INVALID_LABEL_CHARS, '_');

  const handleEvent = (symbol, value) => {

    if (symbol && symbol.includes("Footswitch_")) {
      event.icon.find(`.${symbol}`).prop("checked", value == 1);

    } else if (symbol && symbol.includes("minimum_")) {
      event.icon.find(`[name=${symbol}]`).prop("value", `${value.toFixed(2)} v`);

    } else if (symbol && symbol.includes("maximum_")) {
      event.icon.find(`[name=${symbol}]`).prop("value", `${value.toFixed(2)} v`);
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

  const assignEvents = () => {
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

    footswitchLabelUris.forEach((uri) => funcs.patch_get(uri));

  } else if (event.type == 'change' && event.uri) {
    // Reply to funcs.patch_get(uri): { type: 'change', uri, value }
    handleLabelChanged(event.uri, event.value);

  } else if (event.type == 'change') {
    handleEvent(event.symbol, event.value);

  } else if (event.type == 'patch_set') {
    handleLabelChanged(event.uri, event.value);
  }
}
