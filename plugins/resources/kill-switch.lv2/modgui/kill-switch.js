function (event, funcs) {
  // One function serves every instance of the plugin, so nothing mutable lives in this closure:
  // the state of each instance is kept in event.data.

  // Mirrors PATTERNS in plugins/src/kill-switch.c (and the scale points of `pattern` in kill-switch.ttl): '1' = the
  // signal passes, '0' = it is cut. Keep the three in step
  const PATTERNS = [
    { name: 'Chop 8ths',           steps: '01' },
    { name: 'Chop 16ths',          steps: '01' },
    { name: 'Triplet chop',        steps: '01' },
    { name: 'Machine gun',         steps: '01' },
    { name: 'Syncopated 3-3-2',    steps: '01101101' },
    { name: 'Gallop',              steps: '0110' },
    { name: 'Morello solo chop',   steps: '0101011001011011' },
    { name: 'Morello toggle solo', steps: '0111010100110101' },
    { name: 'Greenwood chunk',     steps: '01110111' },
    { name: 'Buckethead burst',    steps: '0101010111111111' },
  ];
  const SPEEDS = 4;
  const BPM = { min: 40, max: 240 };
  const EDGE = { min: 0.1, max: 20 };          // ms, a logarithmic port: the slider is 0..EDGE_SLIDER
  const EDGE_SLIDER = 1000;
  const DISPLAYED_STEPS = 16;
  const SEND_EVERY_MS = 40;                    // a slider that is being dragged sends at most this often
  const HOLD_DELAY_MS = 400;                   // holding - or + repeats after this, every HOLD_EVERY_MS
  const HOLD_EVERY_MS = 70;

  const clamp = (value, min, max) => Math.max(min, Math.min(max, value));
  const now = () => Date.now();

  const edgeToSlider = (ms) => Math.round(EDGE_SLIDER * Math.log(clamp(ms, EDGE.min, EDGE.max) / EDGE.min) / Math.log(EDGE.max / EDGE.min));
  const sliderToEdge = (position) => EDGE.min * Math.pow(EDGE.max / EDGE.min, clamp(position, 0, EDGE_SLIDER) / EDGE_SLIDER);
  const edgeText = (ms) => `${ms < 10 ? ms.toFixed(1) : Math.round(ms)} ms`;

  // ---------------------------------------------------------------------------
  // State: { pattern: 0.., bpm, speed: 0..3, depth: 0..100, edge: ms, bypassed, lastSent: [symbol] => ms, timers }

  const newState = () => ({
    pattern: 0, bpm: 120, speed: 1, depth: 100, edge: 1.5, bypassed: true,
    lastSent: {}, timers: {},
  });

  // ---------------------------------------------------------------------------
  // Talking to the plugin

  // (MOD UI does not send our own changes back as 'change' events, so what is drawn is what we last sent, until
  // the host says otherwise: a controller assigned to the port, a preset)
  const send = (event, symbol, value) => {
    event.data.state.lastSent[symbol] = now();
    funcs.set_port_value(symbol, value);
  };

  // While a slider is dragged: at most one message every SEND_EVERY_MS, and the last value always gets there
  const sendThrottled = (event, symbol, value) => {
    const state = event.data.state;
    const wait = SEND_EVERY_MS - (now() - (state.lastSent[symbol] || 0));

    clearTimeout(state.timers[symbol]);
    if (wait <= 0) {
      send(event, symbol, value);
    } else {
      state.timers[symbol] = setTimeout(() => send(event, symbol, value), wait);
    }
  };

  const setPattern = (event, index) => {
    const count = PATTERNS.length;
    const pattern = (index % count + count) % count;       // the arrows go round

    event.data.state.pattern = pattern;
    send(event, 'pattern', pattern);
    draw(event);
  };

  const setBpm = (event, bpm) => {
    event.data.state.bpm = clamp(Math.round(bpm), BPM.min, BPM.max);
    send(event, 'bpm', event.data.state.bpm);
    draw(event);
  };

  const setSpeed = (event, speed) => {
    event.data.state.speed = speed;
    send(event, 'speed', speed);
    draw(event);
  };

  // ---------------------------------------------------------------------------
  // Drawing the state

  const stepsHtml = (steps) => {
    const length = steps.length;
    let html = '';

    for (let i = 0; i < DISPLAYED_STEPS; i++) {
      const classes = [
        steps[i % length] === '1' ? 'on' : '',
        i >= length ? 'again' : '',
        i > 0 && i % length === 0 ? 'start' : '',
      ].filter(Boolean).join(' ');
      html += `<i class="${classes}"></i>`;
    }
    return html;
  };

  const setSlider = (input, value, min, max) => {
    input.value = String(value);
    input.style.setProperty('--ks-fill', `${((value - min) / (max - min)) * 100}%`);
  };

  const draw = (event) => {
    const { state, el } = event.data;
    const pattern = PATTERNS[state.pattern] || PATTERNS[0];

    el.root.classList.toggle('is-bypassed', state.bypassed);
    el.state.textContent = state.bypassed ? 'Bypassed' : 'On';
    el.name.textContent = pattern.name;

    if (el.steps.getAttribute('data-pattern') !== String(state.pattern)) {
      el.steps.setAttribute('data-pattern', String(state.pattern));
      el.steps.innerHTML = stepsHtml(pattern.steps);
      el.steps.setAttribute('aria-label', `The steps of ${pattern.name}: ${pattern.steps.replace(/1/g, 'on ').replace(/0/g, 'off ').trim()}`);
    }

    el.bpm.innerHTML = `<b>${state.bpm}</b> BPM`;
    el.segments.forEach((button) => button.setAttribute('aria-checked', String(Number(button.getAttribute('data-speed')) === state.speed)));
    setSlider(el.depth, state.depth, 0, 100);
    el.depthValue.textContent = `${Math.round(state.depth)}%`;
    setSlider(el.edge, edgeToSlider(state.edge), 0, EDGE_SLIDER);
    el.edgeValue.textContent = edgeText(state.edge);
  };

  // ---------------------------------------------------------------------------
  // Events from the user

  const holdToRepeat = (event, button, step) => {
    const stop = () => {
      clearTimeout(event.data.state.timers.hold);
      clearInterval(event.data.state.timers.holdRepeat);
    };

    button.addEventListener('pointerdown', () => {
      stop();
      button.setAttribute('data-pointer', '1');
      step();
      event.data.state.timers.hold = setTimeout(() => {
        event.data.state.timers.holdRepeat = setInterval(step, HOLD_EVERY_MS);
      }, HOLD_DELAY_MS);
    });
    ['pointerup', 'pointercancel', 'pointerleave'].forEach((type) => button.addEventListener(type, stop));

    // A click that follows a press was already handled; one without (the keyboard) is a single step
    button.addEventListener('click', () => {
      if (button.getAttribute('data-pointer')) {
        button.removeAttribute('data-pointer');
      } else {
        step();
      }
    });
  };

  const assignEvents = (event) => {
    const { el } = event.data;
    const state = () => event.data.state;

    el.prev.addEventListener('click', () => setPattern(event, state().pattern - 1));
    el.next.addEventListener('click', () => setPattern(event, state().pattern + 1));
    holdToRepeat(event, el.down, () => setBpm(event, state().bpm - 1));
    holdToRepeat(event, el.up, () => setBpm(event, state().bpm + 1));

    el.segments.forEach((button) => button.addEventListener('click', () => setSpeed(event, Number(button.getAttribute('data-speed')))));

    el.depth.addEventListener('input', () => {
      state().depth = Number(el.depth.value);
      sendThrottled(event, 'depth', state().depth);
      draw(event);
    });
    el.edge.addEventListener('input', () => {
      state().edge = Math.round(sliderToEdge(Number(el.edge.value)) * 100) / 100;
      sendThrottled(event, 'edge', state().edge);
      draw(event);
    });
  };

  const cacheElements = (event) => {
    const root = event.icon.find('.kill-switch')[0];
    const one = (selector) => root.querySelector(selector);

    event.data.el = {
      root,
      state: one('.ks-state'),
      name: one('.ks-pattern-name'),
      steps: one('.ks-steps'),
      prev: one('.ks-prev'),
      next: one('.ks-next'),
      down: one('.ks-bpm-down'),
      up: one('.ks-bpm-up'),
      bpm: one('.ks-bpm'),
      segments: Array.from(root.querySelectorAll('.ks-segment')),
      depth: one('.ks-depth-input'),
      depthValue: one('.ks-depth-value'),
      edge: one('.ks-edge-input'),
      edgeValue: one('.ks-edge-value'),
    };
  };

  // ---------------------------------------------------------------------------
  // Events from the plugin / MOD UI. Returns whether what is drawn may have changed

  const handlePort = (event, symbol, value) => {
    const state = event.data.state;
    const number = Number(value);

    if (symbol === ':bypass') {
      // MOD UI's bypass: 1 = bypassed
      const bypassed = number >= 0.5;
      const changed = state.bypassed !== bypassed;
      state.bypassed = bypassed;
      return changed;
    }

    if (!['pattern', 'bpm', 'speed', 'depth', 'edge'].includes(symbol)) {
      return false;
    }

    if (symbol === 'pattern') state.pattern = clamp(Math.round(number), 0, PATTERNS.length - 1);
    if (symbol === 'bpm') state.bpm = clamp(Math.round(number), BPM.min, BPM.max);
    if (symbol === 'speed') state.speed = clamp(Math.round(number), 0, SPEEDS - 1);
    if (symbol === 'depth') state.depth = clamp(number, 0, 100);
    if (symbol === 'edge') state.edge = clamp(number, EDGE.min, EDGE.max);
    return true;
  };

  if (event.type === 'start') {
    event.data.state = newState();
    cacheElements(event);
    assignEvents(event);

    for (const port of event.ports) {
      handlePort(event, port.symbol, port.value);
    }
    draw(event);

  } else if (event.data.state && event.type === 'change') {
    if (handlePort(event, event.symbol, event.value)) {
      draw(event);
    }
  }
}
