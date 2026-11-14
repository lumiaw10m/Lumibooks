(function () {
    var W = 0, n = 1, page = 0, frac = 0, anchor = null, ready = false, swipeAt = 0, sx = null, sy = 0, wheelAt = 0, timer = 0;
    var cfg = { margin: 16 };
    var h = decodeURIComponent(location.hash.slice(1));
    if (h == 'end') frac = 1;
    else if (h.indexOf('p=') == 0) frac = parseFloat(h.slice(2)) || 0;
    else if (h) anchor = h;

    function send(s) { try { window.external.notify(s); } catch (e) { } }
    function style(id, css) {
        var s = document.getElementById(id);
        if (!s) {
            s = document.createElement('style');
            s.id = id;
            (document.head || document.documentElement).appendChild(s);
        }
        s.textContent = css;
    }

    function go(i, user) {
        page = Math.max(0, Math.min(n - 1, i));
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        document.body.style.transform = 'translateX(' + (-page * W) + 'px)';
        if (user) frac = n > 1 ? page / (n - 1) : 0;
        if (ready) send('page:' + page + '/' + n);
    }

    function pageOf(el) {
        return Math.floor((el.getBoundingClientRect().left + page * W) / W);
    }

    function layout() {
        if (!document.body) return;
        W = window.innerWidth;
        var H = window.innerHeight, m = cfg.margin, v = Math.max(12, Math.round(m * 0.75));
        style('lb-layout', 'body{width:' + W + 'px!important;height:' + H + 'px!important;padding:' + v + 'px ' + m + 'px!important;' +
            'column-width:' + (W - 2 * m) + 'px!important;column-gap:' + (2 * m) + 'px!important}' +
            'img,svg,video{max-height:' + (H - 2 * v) + 'px!important}');
        document.body.style.transform = 'none';
        page = 0;
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        var sw = Math.max(document.documentElement.scrollWidth, document.body.scrollWidth);
        n = Math.max(1, Math.round(sw / W));
        var el = anchor && document.getElementById(anchor);
        if (el) { go(pageOf(el), true); if (ready) anchor = null; }
        else go(Math.round(frac * (n - 1)), false);
    }

    function next() { if (page < n - 1) go(page + 1, true); else send('next-chapter'); }
    function prev() { if (page > 0) go(page - 1, true); else send('prev-chapter'); }

    window.lbApply = function (json) {
        cfg = JSON.parse(json);
        var c = 'html,body{background:' + cfg.bg + '!important;color:' + cfg.fg + '!important}' +
            'body *{color:inherit!important;background-color:transparent!important}' +
            'html{font-size:' + cfg.size + '%!important}body{font-size:1rem!important}' +
            'body,body *{line-height:' + cfg.lh + '!important}' +
            'body p{text-align:' + (cfg.justify ? 'justify' : 'left') + '!important}';
        if (cfg.font) c += 'body,body *{font-family:"' + cfg.font + '"!important}';
        style('lb-settings', c);
        ready = true;
        layout();
        anchor = null;
        document.documentElement.className += ' lb-ready';
    };

    document.addEventListener('click', function (e) {
        if (Date.now() - swipeAt < 500) { e.preventDefault(); return; }
        for (var t = e.target; t && t.getAttribute; t = t.parentNode) {
            var href = t.localName == 'a' && t.getAttribute('href');
            if (href) {
                if (href.charAt(0) == '#') {
                    e.preventDefault();
                    var el = document.getElementById(decodeURIComponent(href.slice(1)));
                    if (el) go(pageOf(el), true);
                }
                return;
            }
        }
        if (e.clientX < W * 0.3) prev();
        else if (e.clientX > W * 0.7) next();
        else send('toggle-ui');
    }, true);

    document.addEventListener('pointerdown', function (e) { sx = e.clientX; sy = e.clientY; }, true);
    document.addEventListener('pointerup', function (e) {
        if (sx === null) return;
        var dx = e.clientX - sx, dy = e.clientY - sy;
        sx = null;
        if (Math.abs(dx) > 40 && Math.abs(dx) > Math.abs(dy)) {
            swipeAt = Date.now();
            if (dx < 0) next(); else prev();
        }
    }, true);

    document.addEventListener('keydown', function (e) {
        var k = e.keyCode;
        if (k == 39 || k == 34 || k == 32) { next(); e.preventDefault(); }
        else if (k == 37 || k == 33) { prev(); e.preventDefault(); }
    });

    document.addEventListener('wheel', function (e) {
        e.preventDefault();
        if (Date.now() - wheelAt < 250) return;
        wheelAt = Date.now();
        if (e.deltaY > 0 || e.deltaX > 0) next(); else prev();
    }, { passive: false });

    window.addEventListener('resize', function () { clearTimeout(timer); timer = setTimeout(layout, 100); });
    window.addEventListener('load', layout);
    document.addEventListener('DOMContentLoaded', layout);
    setTimeout(function () { if (!ready) { ready = true; layout(); document.documentElement.className += ' lb-ready'; } }, 2000);
})();
