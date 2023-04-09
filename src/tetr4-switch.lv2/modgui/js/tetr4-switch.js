function tetr4_switch_set_current(current) {
  document.querySelector(`.dip-switch.active`)?.classList.remove("active");
  document.querySelector(`.dip-switch.group${current}`).classList.add("active");
}