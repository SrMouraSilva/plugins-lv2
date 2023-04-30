function (event, funcs) {
  const handleEvent = (symbol, value) => {

    if (symbol.includes("Footswitch_")) {
      event.icon.find(`.${symbol}`).prop("checked", value == 1);

    } else if (symbol.includes("minimum_")) {
      event.icon.find(`[name=${symbol}]`).prop("value", `${value.toFixed(2)} v`);

    } else if (symbol.includes("maximum_")) {
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

  } else if (event.type == 'change') {
    handleEvent(event.symbol, event.value);
  }
}
