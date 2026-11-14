/* Copyright 2021-2026 Paulo Mateus -- SPDX-License-Identifier: Apache-2.0
   The navigation of the manual: the section being read is marked in the side list, and an image opens larger. */
(function () {
  var links = Array.prototype.slice.call(document.querySelectorAll('.rail a'));
  var byId = {};
  links.forEach(function (a) { byId[a.getAttribute('href').slice(1)] = a; });

  function mark(id) {
    links.forEach(function (a) { a.classList.toggle('is-current', a === byId[id]); });
    var current = byId[id];
    if (current && current.scrollIntoView && getComputedStyle(current.parentNode).flexDirection === 'row') {
      current.parentNode.scrollTo({ left: current.offsetLeft - 24, behavior: 'smooth' });
    }
  }

  if ('IntersectionObserver' in window) {
    var visible = {};
    var observer = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) { visible[entry.target.id] = entry.isIntersecting ? entry.boundingClientRect.top : null; });
      var first = Object.keys(visible).filter(function (id) { return visible[id] !== null; })
        .sort(function (a, b) { return visible[a] - visible[b]; })[0];
      if (first) { mark(first); }
    }, { rootMargin: '-10% 0px -70% 0px' });
    document.querySelectorAll('.sheet[id]').forEach(function (section) { observer.observe(section); });
  }

  var box = document.querySelector('.lightbox');
  var big = box.querySelector('img');
  document.addEventListener('click', function (event) {
    var image = event.target.closest('figure img, .sheet p > img');
    if (image) { big.src = image.src; box.hidden = false; return; }
    if (!box.hidden) { box.hidden = true; }
  });
  document.addEventListener('keydown', function (event) { if (event.key === 'Escape') { box.hidden = true; } });
})();
