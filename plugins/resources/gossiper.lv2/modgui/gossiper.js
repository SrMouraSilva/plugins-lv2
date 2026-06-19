function (event, funcs) {
  const footswitchLabelUris = [
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_1',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_2',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_3',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_4',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_5',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_6',
    'http://srmourasilva.github.io/plugins/gossiper#footswitch_label_7',
  ];

  const handleEvent = (symbol, value) => {

    if (symbol && symbol.includes("Footswitch_")) {
      event.icon.find(`.${symbol}`).prop("checked", value == 1);

    } else if (symbol && symbol.includes("minimum_")) {
      event.icon.find(`[name=${symbol}]`).prop("value", `${value.toFixed(2)} v`);

    } else if (symbol && symbol.includes("maximum_")) {
      event.icon.find(`[name=${symbol}]`).prop("value", `${value.toFixed(2)} v`);
    }
  }

  const assignEvents = () => {}

  if (event.type == 'start') {
    assignEvents();

    const ports = event.ports;

    for (let p in ports) {
      handleEvent(ports[p].symbol, ports[p].value);
    }

    footswitchLabelUris.forEach((uri) => funcs.patch_get(uri));

  } else if (event.type == 'change') {
    handleEvent(event.symbol, event.value);
  }
}
