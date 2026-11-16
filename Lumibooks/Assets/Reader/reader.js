(function () {
    var XHTML = 'http://www.w3.org/1999/xhtml';
    var W = 0, n = 1, page = 0, frac = 0, anchor = null, offset = null, query = null, found = null, note = null, ready = false, timer = 0;
    var sx = null, sy = 0, sliding = false, swipeAt = 0, wheelAt = 0, pressTimer = 0, quietUntil = 0;
    var cfg = { margin: 16, vmargin: 20 };
    var sents = [], spoken = -1, sel = null, tn = null, ov = null, hA = null, hB = null, drag = null, fixed = null;
    var BLOCK = /^(p|div|h[1-6]|li|blockquote|td|th|dd|dt|pre|section|article|aside|header|footer|figcaption|caption|body|tr|table|ul|ol|dl|figure|nav|main|address)$/;
    var SKIP = /^(script|style|rt|rp|noscript|svg|math)$/;
    var CLOSE = /["'\u201d\u2019\u00bb)\]]/;
    var WORD = /[^\s.,;:!?"\u201c\u201d\u2018()\[\]{}\u00ab\u00bb\u2014\u2013\u2026]+/g;
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
        query = null;
        if (h == 'end') frac = 1;
        else if (h.indexOf('p=') == 0) frac = parseFloat(h.slice(2)) || 0;
        else if (h.indexOf('o=') == 0) offset = parseInt(h.slice(2), 10) || 0;
        else if (h.indexOf('q=') == 0) { var c = h.indexOf(':'); query = { k: parseInt(h.slice(2, c), 10) || 0, t: h.slice(c + 1).toLowerCase() }; }
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

    function go(i, user, anim) {
        page = Math.max(0, Math.min(n - 1, i));
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        anim = anim && cfg.anim !== false;
        document.body.style.transition = anim ? 'transform .25s ease-out' : 'none';
        document.body.style.transform = 'translateX(' + (-page * W) + 'px)';
        if (user) frac = n > 1 ? page / (n - 1) : 0;
        if (ready) send('page:' + page + '/' + n);
        if (sel || found || ov) { if (anim) setTimeout(drawSel, 270); else drawSel(); }
    }

    function layout() {
        if (!document.body) return;
        W = window.innerWidth;
        var H = window.innerHeight, m = cfg.margin, v = cfg.vmargin;
        style('lb-layout', 'body{width:' + W + 'px!important;height:' + H + 'px!important;padding:' + v + 'px ' + m + 'px!important;' +
            'column-width:' + (W - 2 * m) + 'px!important;column-gap:' + (2 * m) + 'px!important}' +
            'img,svg,video{max-height:' + (H - 2 * v) + 'px!important}');
        document.body.style.transition = 'none';
        document.body.style.transform = 'none';
        page = 0;
        window.scrollTo(0, 0);
        document.body.scrollLeft = 0;
        n = Math.max(1, Math.round(Math.max(document.documentElement.scrollWidth, document.body.scrollWidth) / W));
        var el = anchor && document.getElementById(anchor);
        if (el) go(pageOf(el), true);
        else if (offset !== null) go(pageOfOffset(offset), true);
        else if (query) {
            found = findQuery(query);
            if (found) go(pageOfOffset(found.s), true);
            else go(0, true);
        }
        else go(Math.round(frac * (n - 1)), false);
        if (ready) { anchor = null; offset = null; query = null; }
    }

    function next() { if (page < n - 1) go(page + 1, true, true); else send('next-chapter'); }
    function prev() { if (page > 0) go(page - 1, true, true); else send('prev-chapter'); }

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
        tn = null;
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

    function nodes() { return tn || (tn = textNodes()); }

    function el(tag, cls) {
        var e = document.createElementNS(XHTML, tag);
        if (cls) e.className = cls;
        return e;
    }

    function pieces(s, e, fn) {
        nodes().forEach(function (x) {
            var v = x.n.nodeValue, a = Math.max(s, x.at) - x.at, b = Math.min(e, x.at + v.length) - x.at;
            if (b > a && /\S/.test(v.slice(a, b))) fn(x.n, a, b);
        });
    }

    function wordAt(x, y) {
        var e = document.elementFromPoint(x, y);
        if (!e || e == document.body || !document.body.contains(e)) return null;
        while (e.parentNode != document.body && !BLOCK.test(e.localName)) e = e.parentNode;
        var l = nodes(), r = document.createRange(), best = null, bd = 1e9;
        for (var i = 0; i < l.length; i++) {
            var t = l[i].n, v = t.nodeValue, m;
            if (!e.contains(t)) continue;
            WORD.lastIndex = 0;
            while ((m = WORD.exec(v))) {
                r.setStart(t, m.index);
                r.setEnd(t, m.index + m[0].length);
                var q = r.getClientRects();
                for (var k = 0; k < q.length; k++) {
                    var c = q[k], dx = x < c.left ? c.left - x : x > c.right ? x - c.right : 0, dy = y < c.top ? c.top - y : y > c.bottom ? y - c.bottom : 0;
                    if (dy * 4 + dx >= bd) continue;
                    bd = dy * 4 + dx;
                    best = { s: l[i].at + m.index, e: l[i].at + m.index + m[0].length };
                    if (!bd) return best;
                }
            }
        }
        return best;
    }

    function place(h, c, x) {
        h.style.cssText = c ? 'display:block!important;left:' + x + 'px!important;top:' + c.bottom + 'px!important' : 'display:none!important';
    }

    function drawSel() {
        if (!ov) {
            ov = el('div', 'lb-ov');
            hA = el('div', 'lb-hd lb-ha');
            hB = el('div', 'lb-hd lb-hb');
            hA.lbH = 'a';
            hB.lbH = 'b';
            [ov, hA, hB].forEach(function (x) { document.documentElement.appendChild(x); });
        }
        while (ov.firstChild) ov.removeChild(ov.firstChild);
        var f = null, z = null, r = document.createRange();
        if (found) pieces(found.s, found.e, function (t, a, b) {
            r.setStart(t, a);
            r.setEnd(t, b);
            var q = r.getClientRects();
            for (var k = 0; k < q.length; k++) {
                var c = q[k];
                if (c.width < 1 || c.right < 0 || c.left > W) continue;
                var d = el('i', 'lb-found');
                d.style.cssText = 'left:' + c.left + 'px;top:' + c.top + 'px;width:' + c.width + 'px;height:' + c.height + 'px';
                ov.appendChild(d);
            }
        });
        if (sel) pieces(sel.s, sel.e, function (t, a, b) {
            r.setStart(t, a);
            r.setEnd(t, b);
            var q = r.getClientRects();
            for (var k = 0; k < q.length; k++) {
                var c = q[k];
                if (c.width < 1 || c.right < 0 || c.left > W) continue;
                var d = el('i');
                d.style.cssText = 'left:' + c.left + 'px;top:' + c.top + 'px;width:' + c.width + 'px;height:' + c.height + 'px';
                ov.appendChild(d);
                if (!f) f = c;
                z = c;
            }
        });
        place(hA, f, f && f.left - 32);
        place(hB, z, z && z.right - 12);
    }

    function selDone() {
        if (!sel) return;
        var t = [];
        pieces(sel.s, sel.e, function (n, a, b) { t.push(n.nodeValue.slice(a, b)); });
        send('sel:' + JSON.stringify({ s: sel.s, e: sel.e, t: t.join('').replace(/\s+/g, ' ').trim() }));
    }

    function snap() {
        if (window.pageXOffset || window.pageYOffset) window.scrollTo(0, 0);
        var b = document.body, d = document.documentElement;
        if (b && (b.scrollLeft || b.scrollTop)) { b.scrollLeft = 0; b.scrollTop = 0; }
        if (d && (d.scrollLeft || d.scrollTop)) { d.scrollLeft = 0; d.scrollTop = 0; }
        if (b && !sliding) {
            b.style.transition = 'none';
            b.style.transform = 'translateX(' + (-page * W) + 'px)';
        }
    }

    window.lbSnap = function () { snap(); if (sel || found || ov) drawSel(); return ''; };

    window.lbSelClear = function () { sel = null; drag = null; snap(); drawSel(); };

    function findQuery(q) {
        var l = nodes(), text = '';
        l.forEach(function (x) { text += x.n.nodeValue; });
        text = text.toLowerCase();
        var at = -1, last = -1;
        for (var i = 0; i <= q.k; i++) {
            at = text.indexOf(q.t, at + 1);
            if (at < 0) break;
            last = at;
        }
        return last < 0 ? null : { s: last, e: last + q.t.length };
    }

    function closeNote() {
        if (!note) return false;
        note.parentNode.removeChild(note);
        note = null;
        send('note:0');
        return true;
    }

    window.lbCloseNote = function () { return closeNote() ? '1' : ''; };

    function isNoteLink(a) {
        var type = (a.getAttribute('epub:type') || a.getAttributeNS('http://www.idpf.org/2007/ops', 'type') || a.getAttribute('role') || '').toLowerCase();
        if (type.indexOf('noteref') >= 0) return true;
        var p = a.parentNode;
        if (p && (p.localName == 'sup' || p.localName == 'sub')) return true;
        if (a.querySelector && a.querySelector('sup')) return true;
        var t = (a.textContent || '').replace(/\s+/g, '');
        return /^[\[\(]?([0-9]{1,3}|[*\u2020\u2021]+|[ivx]{1,5}|[a-z])[\]\)]?$/i.test(t);
    }

    function noteContent(target) {
        var out = document.createDocumentFragment();
        var src = target;
        if (/^(a|span|sup)$/.test(target.localName)) {
            var p = target;
            while (p && p.parentNode && !BLOCK.test(p.localName)) p = p.parentNode;
            src = p || target;
        }
        var kids = src.localName == 'body' ? [target] : [src];
        kids.forEach(function (k) { out.appendChild(document.importNode(k, true)); });
        var tmp = el('div');
        tmp.appendChild(out);
        var scripts = tmp.getElementsByTagName('script');
        for (var i = scripts.length - 1; i >= 0; i--) scripts[i].parentNode.removeChild(scripts[i]);
        return tmp;
    }

    function showNote(content, href) {
        closeNote();
        note = el('div', 'lb-note');
        note.style.cssText = 'background:' + (cfg.bg || '#fff') + ';color:' + (cfg.fg || '#111') + ';font-size:' + (cfg.size || 100) + '%';
        var body = el('div', 'lb-note-body');
        body.appendChild(content);
        var link = el('div', 'lb-note-go');
        link.textContent = 'Go to note';
        link.lbHref = href;
        note.appendChild(body);
        note.appendChild(link);
        document.documentElement.appendChild(note);
        send('note:1');
    }

    function openNote(a, href) {
        var hash = href.indexOf('#');
        if (hash < 0) return false;
        var id = decodeURIComponent(href.slice(hash + 1)), file = href.slice(0, hash);
        if (!file) {
            var t = document.getElementById(id);
            if (!t) return false;
            showNote(noteContent(t), href);
            return true;
        }
        var x = new XMLHttpRequest();
        x.open('GET', new URL(file, location.href).href, true);
        x.onload = function () {
            var doc = null;
            try { doc = new DOMParser().parseFromString(x.responseText, /\.x?html?$/i.test(file) && /^<\?xml/.test(x.responseText) ? 'application/xhtml+xml' : 'text/html'); } catch (err) { }
            if (!doc || doc.getElementsByTagName('parsererror').length) {
                try { doc = new DOMParser().parseFromString(x.responseText, 'text/html'); } catch (err) { doc = null; }
            }
            var t = doc && (doc.getElementById(id) || doc.querySelector('[name="' + id.replace(/"/g, '') + '"]'));
            if (t && (t.textContent || '').length < 4000) showNote(noteContent(t), href);
            else location.href = href;
        };
        x.onerror = function () { location.href = href; };
        x.send();
        return true;
    }


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
        tn = null;
        if (sel || found) drawSel();
    };

    document.addEventListener('click', function (e) {
        if (Date.now() < quietUntil || Date.now() - swipeAt < 500 || e.target.lbH) { e.preventDefault(); return; }
        if (note) {
            for (var n0 = e.target; n0; n0 = n0.parentNode) {
                if (n0.lbHref) { var target = n0.lbHref; closeNote(); e.preventDefault(); if (target.charAt(0) == '#') { var te = document.getElementById(decodeURIComponent(target.slice(1))); if (te) go(pageOf(te), true); } else location.href = target; return; }
                if (n0 == note) { e.preventDefault(); return; }
            }
            closeNote();
            e.preventDefault();
            return;
        }
        if (sel || found) {
            var had = !!sel;
            sel = null;
            found = null;
            drawSel();
            if (had) { send('sel-clear'); e.preventDefault(); return; }
        }
        for (var t = e.target; t && t.getAttribute; t = t.parentNode) {
            var href = t.localName == 'a' && t.getAttribute('href');
            if (href) {
                if (isNoteLink(t) && openNote(t, href)) { e.preventDefault(); return; }
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
        clearTimeout(pressTimer);
        if (e.target.lbH) {
            drag = e.target.lbH;
            sx = null;
            e.preventDefault();
            return;
        }
        sx = e.clientX;
        sy = e.clientY;
        pressTimer = setTimeout(function () {
            var w = sx === null ? null : wordAt(sx, sy);
            if (!w) return;
            sx = null;
            sel = { s: w.s, e: w.e };
            fixed = w;
            drag = 'x';
            drawSel();
        }, 500);
    }, true);

    document.addEventListener('pointermove', function (e) {
        if (drag) {
            var w = wordAt(e.clientX, e.clientY - (drag == 'x' ? 0 : 28));
            if (w) {
                if (drag == 'a') { if (w.s < sel.e) sel.s = w.s; }
                else if (drag == 'b') { if (w.e > sel.s) sel.e = w.e; }
                else { sel.s = Math.min(fixed.s, w.s); sel.e = Math.max(fixed.e, w.e); }
                drawSel();
            }
            e.preventDefault();
            return;
        }
        if (sx === null) return;
        var mx = e.clientX - sx, my = e.clientY - sy;
        if (Math.abs(mx) + Math.abs(my) > 12) clearTimeout(pressTimer);
        if (cfg.anim === false || note || sel) return;
        if (!sliding && Math.abs(mx) > 12 && Math.abs(mx) > Math.abs(my)) sliding = true;
        if (!sliding) return;
        var off = (page == 0 && mx > 0) || (page == n - 1 && mx < 0) ? mx * 0.35 : mx;
        document.body.style.transition = 'none';
        document.body.style.transform = 'translateX(' + (off - page * W) + 'px)';
    }, true);

    document.addEventListener('pointercancel', function () {
        clearTimeout(pressTimer);
        sx = null;
        if (sliding) { sliding = false; go(page, false, true); }
        if (drag) { drag = null; selDone(); }
    }, true);

    document.addEventListener('pointerup', function (e) {
        clearTimeout(pressTimer);
        if (drag) {
            drag = null;
            quietUntil = Date.now() + 400;
            selDone();
            return;
        }
        if (sx === null) return;
        var dx = e.clientX - sx, dy = e.clientY - sy, was = sliding;
        sx = null;
        sliding = false;
        if (Math.abs(dx) > 40 && Math.abs(dx) > Math.abs(dy)) {
            swipeAt = Date.now();
            if (dx < 0) next(); else prev();
            if (was && ((dx < 0 && page == n - 1) || (dx > 0 && page == 0))) go(page, false, true);
        }
        else if (was) {
            swipeAt = Date.now();
            go(page, false, true);
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
    document.addEventListener('scroll', function () { if (!sliding) snap(); }, true);
    window.addEventListener('blur', function () { setTimeout(snap, 50); });
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
