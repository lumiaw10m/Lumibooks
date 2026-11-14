(function () {
    var XHTML = 'http://www.w3.org/1999/xhtml';
    var W = 0, n = 1, page = 0, frac = 0, anchor = null, offset = null, ready = false, timer = 0;
    var sx = null, sy = 0, swipeAt = 0, wheelAt = 0, pressTimer = 0, pressed = false;
    var cfg = { margin: 16, vmargin: 20 };
    var sents = [], spoken = -1, sel = null;
    var BLOCK = /^(p|div|h[1-6]|li|blockquote|td|th|dd|dt|pre|section|article|aside|header|footer|figcaption|caption|body|tr|table|ul|ol|dl|figure|nav|main|address)$/;
    var SKIP = /^(script|style|rt|rp|noscript|svg|math)$/;
    var CLOSE = /["'\u201d\u2019\u00bb)\]]/;
    var ABBR = /^(mr|mrs|ms|dr|st|jr|sr|prof|vs|sra|dra|mme|mlle|no|vol|ch|fig)$/i;

    function send(s) { try { window.external.notify(s); } catch (e) { } }

    function style(id, css) {
        var s = document.getElementById(id);
        if (!s) {
            s = document.createElementNS(XHTML, 'style');
            s.id = id;
            (document.head || document.documentElement).appendChild(s);
        }
        s.textContent = css;
    }

    function span(cls) {
        var e = document.createElementNS(XHTML, 'span');
        e.className = cls;
        return e;
    }

    function parseHash() {
        var h = decodeURIComponent(location.hash.slice(1));
        anchor = null;
        offset = null;
        if (h == 'end') frac = 1;
        else if (h.indexOf('p=') == 0) frac = parseFloat(h.slice(2)) || 0;
        else if (h.indexOf('o=') == 0) offset = parseInt(h.slice(2), 10) || 0;
        else if (h) anchor = h;
    }

    function textNodes() {
        var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, null, false), out = [], t, at = 0;
        while ((t = walker.nextNode())) {
            var p = t.parentNode.localName;
            if (p == 'script' || p == 'style') continue;
            out.push({ n: t, at: at });
            at += t.nodeValue.length;
        }
        return out;
    }

    function pageAt(left) { return Math.floor((left + page * W) / W); }
    function pageOf(el) { return pageAt(el.getBoundingClientRect().left); }

    function pageOfOffset(o) {
        var list = textNodes();
        for (var i = 0; i < list.length; i++) {
            var r = list[i], len = r.n.nodeValue.length;
            if (!len || (o >= r.at + len && i < list.length - 1)) continue;
            var k = Math.max(0, Math.min(o - r.at, len - 1)), range = document.createRange();
            range.setStart(r.n, k);
            range.setEnd(r.n, k + 1);
            var rects = range.getClientRects();
            return rects.length ? pageAt(rects[0].left) : pageOf(r.n.parentNode);
        }
        return 0;
    }

    function go(i, user) {
        page = Math.max(0, Math.min(n - 1, i));
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        document.body.style.transform = 'translateX(' + (-page * W) + 'px)';
        if (user) frac = n > 1 ? page / (n - 1) : 0;
        if (ready) send('page:' + page + '/' + n);
    }

    function layout() {
        if (!document.body) return;
        W = window.innerWidth;
        var H = window.innerHeight, m = cfg.margin, v = cfg.vmargin;
        style('lb-layout', 'body{width:' + W + 'px!important;height:' + H + 'px!important;padding:' + v + 'px ' + m + 'px!important;' +
            'column-width:' + (W - 2 * m) + 'px!important;column-gap:' + (2 * m) + 'px!important}' +
            'img,svg,video{max-height:' + (H - 2 * v) + 'px!important}');
        document.body.style.transform = 'none';
        page = 0;
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        n = Math.max(1, Math.round(Math.max(document.documentElement.scrollWidth, document.body.scrollWidth) / W));
        var el = anchor && document.getElementById(anchor);
        if (el) go(pageOf(el), true);
        else if (offset !== null) go(pageOfOffset(offset), true);
        else go(Math.round(frac * (n - 1)), false);
        if (ready) { anchor = null; offset = null; }
    }

    function next() { if (page < n - 1) go(page + 1, true); else send('next-chapter'); }
    function prev() { if (page > 0) go(page - 1, true); else send('prev-chapter'); }

    window.lbApply = function (json) {
        cfg = JSON.parse(json);
        var c = 'html,body{background:' + cfg.bg + '!important;color:' + cfg.fg + '!important}' +
            'body *{color:inherit!important;background-color:transparent!important}' +
            'html{font-size:' + cfg.size + '%!important}body{font-size:1rem!important}' +
            'body,body *{line-height:' + cfg.lh + '!important;-ms-hyphens:' + (cfg.hyph ? 'auto' : 'manual') + '!important;hyphens:' + (cfg.hyph ? 'auto' : 'manual') + '!important}' +
            'body p{text-align:' + (cfg.justify ? 'justify' : 'left') + '!important}';
        if (cfg.font) c += 'body,body *{font-family:"' + cfg.font + '"!important}';
        if (cfg.bold) c += 'body,body *{font-weight:bold!important}';
        if (cfg.ls) c += 'body,body *{letter-spacing:' + cfg.ls / 100 + 'em!important}';
        if (cfg.para >= 0) c += 'body p{margin-top:0!important;margin-bottom:' + cfg.para / 10 + 'em!important}';
        if (cfg.indent >= 0) c += 'body p{text-indent:' + cfg.indent / 10 + 'em!important}';
        style('lb-settings', c);
        ready = true;
        layout();
        document.documentElement.classList.add('lb-ready');
    };

    function blockOf(t) {
        for (var e = t.parentNode; e && e.localName; e = e.parentNode) {
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
        var segs = [], seg = null;
        textNodes().forEach(function (r) {
            var b = blockOf(r.n);
            if (!b) return;
            if (!seg || seg.b != b) { seg = { b: b, nodes: [], text: '' }; segs.push(seg); }
            seg.nodes.push({ n: r.n, g: r.at, at: seg.text.length, cuts: [] });
            seg.text += r.n.nodeValue;
        });
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
                    var idx = sents.length, S = { t: t.slice(s, z), e: [], s: -1, z: -1 };
                    sents.push(S);
                    sg.nodes.forEach(function (r) {
                        var ns = Math.max(s, r.at) - r.at, ne = Math.min(z, r.at + r.n.nodeValue.length) - r.at;
                        if (ne <= ns) return;
                        r.cuts.push({ s: ns, e: ne, i: idx });
                        if (S.s < 0) S.s = r.g + ns;
                        S.z = r.g + ne;
                    });
                }
                a = e;
            }
            sg.nodes.forEach(function (r) {
                if (!r.cuts.length) return;
                var v = r.n.nodeValue, pos = 0, f = document.createDocumentFragment();
                r.cuts.forEach(function (c) {
                    if (c.s > pos) f.appendChild(document.createTextNode(v.slice(pos, c.s)));
                    var sp = span('lb-s');
                    sp.textContent = v.slice(c.s, c.e);
                    sp.lbI = c.i;
                    f.appendChild(sp);
                    sents[c.i].e.push(sp);
                    pos = c.e;
                });
                if (pos < v.length) f.appendChild(document.createTextNode(v.slice(pos)));
                r.n.parentNode.replaceChild(f, r.n);
            });
        });
    }

    function sentences() {
        if (!sents.length) build();
        return sents;
    }

    function pageOfSentence(i) { return pageOf(sents[i].e[0]); }

    function firstOnPage() {
        var i = 0;
        while (i < sentences().length && pageOfSentence(i) < page) i++;
        return Math.max(0, Math.min(i, sents.length - 1));
    }

    function sentenceAt(x, y) {
        sentences();
        for (var e = document.elementFromPoint(x, y); e && e.localName; e = e.parentNode)
            if (e.lbI !== undefined) return e.lbI;
        return -1;
    }

    function mark(i, cls, on) {
        if (sents[i]) sents[i].e.forEach(function (e) { e.classList[on ? 'add' : 'remove'](cls); });
    }

    window.lbSentences = function (from) {
        var start = from == 'page' ? firstOnPage() : 0;
        return JSON.stringify({ start: start, s: sentences().map(function (x) { return x.t; }) });
    };

    window.lbHighlight = function (i) {
        mark(spoken, 'lb-hl', false);
        spoken = +i;
        if (!sents[spoken]) return;
        mark(spoken, 'lb-hl', true);
        var p = pageOfSentence(spoken);
        if (p != page) go(p, true);
    };

    window.lbSnippet = function () {
        var s = sentences();
        return s.length ? s[firstOnPage()].t.slice(0, 160) : '';
    };

    function showSel() {
        var old = document.querySelectorAll('.lb-sel');
        for (var i = 0; i < old.length; i++) old[i].classList.remove('lb-sel');
        if (!sel) return;
        var text = [];
        for (i = sel.a; i <= sel.b; i++) { mark(i, 'lb-sel', true); text.push(sents[i].t); }
        send('sel:' + JSON.stringify({ s: sents[sel.a].s, e: sents[sel.b].z, t: text.join(' ') }));
    }

    function extend(i) {
        if (!sel) sel = { a: i, b: i };
        else if (i < sel.a) sel.a = i;
        else sel.b = i;
        showSel();
    }

    window.lbSelClear = function () { sel = null; showSel(); };

    function isBlock(e) { return !e || (e.nodeType == 1 && BLOCK.test(e.localName)); }

    function wrap(s, e, make) {
        textNodes().forEach(function (r) {
            var len = r.n.nodeValue.length, a = Math.max(s, r.at) - r.at, b = Math.min(e, r.at + len) - r.at;
            if (b <= a || (!/\S/.test(r.n.nodeValue) && BLOCK.test(r.n.parentNode.localName) && isBlock(r.n.previousSibling) && isBlock(r.n.nextSibling))) return;
            var t = r.n;
            if (a > 0) t = t.splitText(a);
            if (b - a < t.nodeValue.length) t.splitText(b - a);
            var sp = make();
            t.parentNode.replaceChild(sp, t);
            sp.appendChild(t);
        });
    }

    window.lbHighlights = function (json) {
        var old = document.querySelectorAll('span.lb-h');
        for (var i = 0; i < old.length; i++) {
            var o = old[i], p = o.parentNode;
            while (o.firstChild) p.insertBefore(o.firstChild, o);
            p.removeChild(o);
        }
        JSON.parse(json).forEach(function (h) {
            wrap(h.s, h.e, function () {
                var e = span('lb-h lb-h-' + h.c + (h.n ? ' lb-hn' : ''));
                e.setAttribute('data-h', h.id);
                return e;
            });
        });
    };

    document.addEventListener('click', function (e) {
        if (pressed) { pressed = false; e.preventDefault(); return; }
        if (Date.now() - swipeAt < 500) { e.preventDefault(); return; }
        if (sel) {
            var i = sentenceAt(e.clientX, e.clientY);
            if (i >= 0) extend(i);
            else { sel = null; showSel(); send('sel-clear'); }
            e.preventDefault();
            return;
        }
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
            var h = t.getAttribute('data-h');
            if (h) { send('hl:' + h); return; }
        }
        if (e.clientX < W * 0.3) prev();
        else if (e.clientX > W * 0.7) next();
        else send('toggle-ui');
    }, true);

    document.addEventListener('pointerdown', function (e) {
        sx = e.clientX;
        sy = e.clientY;
        pressed = false;
        clearTimeout(pressTimer);
        pressTimer = setTimeout(function () {
            var i = sentenceAt(sx, sy);
            sx = null;
            if (i < 0) return;
            pressed = true;
            extend(i);
        }, 550);
    }, true);

    document.addEventListener('pointermove', function (e) {
        if (sx !== null && Math.abs(e.clientX - sx) + Math.abs(e.clientY - sy) > 12) clearTimeout(pressTimer);
    }, true);

    document.addEventListener('pointercancel', function () { clearTimeout(pressTimer); sx = null; }, true);

    document.addEventListener('pointerup', function (e) {
        clearTimeout(pressTimer);
        if (sx === null) return;
        var dx = e.clientX - sx, dy = e.clientY - sy;
        sx = null;
        if (Math.abs(dx) > 40 && Math.abs(dx) > Math.abs(dy)) {
            swipeAt = Date.now();
            if (dx < 0) next(); else prev();
        }
    }, true);

    document.addEventListener('contextmenu', function (e) { e.preventDefault(); }, true);

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

    parseHash();
    window.addEventListener('hashchange', function () { parseHash(); if (ready) layout(); });
    window.addEventListener('resize', function () { clearTimeout(timer); timer = setTimeout(layout, 100); });
    window.addEventListener('load', layout);
    document.addEventListener('DOMContentLoaded', layout);
    setTimeout(function () {
        if (ready) return;
        ready = true;
        layout();
        document.documentElement.classList.add('lb-ready');
    }, 2000);
})();
