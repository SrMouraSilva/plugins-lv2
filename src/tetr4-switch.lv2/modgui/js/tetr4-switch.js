function (event, funcs) {
  const handleEvent = (symbol, value) => {
    if (symbol.includes("Preset_") && symbol.includes("_output_")) {
      event.icon.find(`label[data-switch=${symbol}] input`).prop("checked", value == 1);
    }

    if (symbol.includes("Preset_selector_")) {
      const id = symbol.charAt(symbol.length - 1);
      event.icon.find(`.${symbol}`).prop("checked", value == 1);

      if (value == 1) {
        event.icon.find(`.dip-switch.active`)?.removeClass("active");
        event.icon.find(`.dip-switch.group${id}`).addClass("active");
      }
    }
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
  }

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
