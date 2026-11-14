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

    var sents = [], hl = -1;
    var BLOCK = /^(p|div|h[1-6]|li|blockquote|td|th|dd|dt|pre|section|article|aside|header|footer|figcaption|caption|body|tr|table|ul|ol|dl|figure|nav|main|address)$/;
    var SKIP = /^(script|style|rt|rp|noscript|svg|math)$/;
    var CLOSE = /["'\u201d\u2019\u00bb)\]]/;
    var ABBR = /^(mr|mrs|ms|dr|st|jr|sr|prof|vs|sra|dra|mme|mlle|no|vol|ch|fig)$/i;

    function blockOf(n) {
        for (var e = n.parentNode; e && e.localName; e = e.parentNode) {
            if (SKIP.test(e.localName)) return null;
            if (BLOCK.test(e.localName)) return e;
        }
        return null;
    }

    function isEnd(t, i) {
        var c = t.charAt(i);
        if ('.!?\u2026\u3002\uff01\uff1f'.indexOf(c) < 0) return false;
        var j = i + 1;
        while (j < t.length && CLOSE.test(t.charAt(j))) j++;
        if (j < t.length && !/\s/.test(t.charAt(j))) return false;
        var k = j;
        while (k < t.length && /\s/.test(t.charAt(k))) k++;
        if (k < t.length && /[a-z\u00df-\u00ff]/.test(t.charAt(k))) return false;
        if (c == '.') {
            var m = /([^\s.]+)$/.exec(t.slice(Math.max(0, i - 12), i));
            var w = m ? m[1].replace(/^\W+/, '') : '';
            if (/^[A-Z]$/.test(w) || ABBR.test(w)) return false;
        }
        return true;
    }

    function build() {
        var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null, false), n, segs = [], seg = null;
        while ((n = walker.nextNode())) {
            var b = blockOf(n);
            if (!b) continue;
            if (!seg || seg.b != b) { seg = { b: b, nodes: [], text: '' }; segs.push(seg); }
            seg.nodes.push({ n: n, at: seg.text.length, cuts: [] });
            seg.text += n.nodeValue;
        }
        segs.forEach(function (sg) {
            var t = sg.text, L = t.length, a = 0;
            while (a < L) {
                var e = a;
                while (e < L && !isEnd(t, e)) e++;
                if (e < L) { e++; while (e < L && CLOSE.test(t.charAt(e))) e++; }
                var s = a, z = e;
                while (s < z && /\s/.test(t.charAt(s))) s++;
                while (z > s && /\s/.test(t.charAt(z - 1))) z--;
                if (z > s && /[0-9A-Za-z\u00c0-\uffff]/.test(t.slice(s, z))) {
                    var idx = sents.length;
                    sents.push({ t: t.slice(s, z), e: [] });
                    sg.nodes.forEach(function (r) {
                        var ns = Math.max(s, r.at) - r.at, ne = Math.min(z, r.at + r.n.nodeValue.length) - r.at;
                        if (ne > ns) r.cuts.push({ s: ns, e: ne, i: idx });
                    });
                }
                a = e;
            }
            sg.nodes.forEach(function (r) {
                if (!r.cuts.length) return;
                var v = r.n.nodeValue, pos = 0, f = document.createDocumentFragment();
                r.cuts.forEach(function (c) {
                    if (c.s > pos) f.appendChild(document.createTextNode(v.slice(pos, c.s)));
                    var sp = document.createElementNS('http://www.w3.org/1999/xhtml', 'span');
                    sp.className = 'lb-s';
                    sp.textContent = v.slice(c.s, c.e);
                    f.appendChild(sp);
                    sents[c.i].e.push(sp);
                    pos = c.e;
                });
                if (pos < v.length) f.appendChild(document.createTextNode(v.slice(pos)));
                r.n.parentNode.replaceChild(f, r.n);
            });
        });
    }

    function pageOfSentence(i) {
        return pageOf(sents[i].e[0]);
    }

    window.lbSentences = function (from) {
        if (!sents.length) build();
        var start = 0;
        if (from == 'page') {
            while (start < sents.length && pageOfSentence(start) < page) start++;
            start = Math.max(0, Math.min(start, sents.length - 1));
        }
        return JSON.stringify({ start: start, s: sents.map(function (x) { return x.t; }) });
    };

    window.lbHighlight = function (i) {
        i = +i;
        if (sents[hl]) sents[hl].e.forEach(function (e) { e.className = 'lb-s'; });
        hl = i;
        if (!sents[i]) return;
        sents[i].e.forEach(function (e) { e.className = 'lb-s lb-hl'; });
        var p = pageOfSentence(i);
        if (p != page) go(p, true);
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
