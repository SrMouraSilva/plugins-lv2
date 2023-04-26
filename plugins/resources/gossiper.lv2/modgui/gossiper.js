function (event, funcs) {
  const handleEvent = (symbol, value) => {
    if (symbol.includes("Footswitch_")) {
      const id = symbol.charAt(symbol.length - 1);
      event.icon.find(`.${symbol}`).prop("checked", value == 1);
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
