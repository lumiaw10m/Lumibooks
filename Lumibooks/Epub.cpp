#include "pch.h"
#include "Epub.h"
#include <map>

using namespace Lumibooks;
using namespace Platform;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::Storage::Streams;

std::string Lumibooks::Utf8(const wchar_t* s)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string r(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, &r[0], n, nullptr, nullptr);
    return r;
}

std::wstring Lumibooks::Wide(const char* s, size_t n)
{
    if (!n) return std::wstring();
    int w = MultiByteToWideChar(CP_UTF8, 0, s, (int)n, nullptr, 0);
    std::wstring r(w, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, (int)n, &r[0], w);
    return r;
}

static int Hex(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string Lumibooks::UrlDecode(const std::string& s)
{
    std::string r;
    for (size_t i = 0; i < s.size(); i++)
    {
        int a, b;
        if (s[i] == '%' && i + 2 < s.size() && (a = Hex(s[i + 1])) >= 0 && (b = Hex(s[i + 2])) >= 0)
        {
            r += (char)(a * 16 + b);
            i += 2;
        }
        else r += s[i];
    }
    return r;
}

std::string Lumibooks::JoinPath(const std::string& dir, const std::string& href)
{
    std::string full = (!href.empty() && href[0] == '/') ? href.substr(1) : dir + href;
    size_t hash = full.find('#');
    if (hash != std::string::npos) full.resize(hash);
    std::vector<std::string> parts;
    size_t start = 0;
    while (start <= full.size())
    {
        size_t end = full.find('/', start);
        if (end == std::string::npos) end = full.size();
        std::string p = full.substr(start, end - start);
        if (p == "..") { if (!parts.empty()) parts.pop_back(); }
        else if (!p.empty() && p != ".") parts.push_back(p);
        start = end + 1;
    }
    std::string r;
    for (auto& p : parts) { if (!r.empty()) r += '/'; r += p; }
    return r;
}

IBuffer^ Lumibooks::ToBuffer(const uint8_t* p, size_t n)
{
    auto w = ref new DataWriter();
    if (n) w->WriteBytes(ArrayReference<unsigned char>(const_cast<uint8_t*>(p), (unsigned int)n));
    return w->DetachBuffer();
}

std::vector<uint8_t> Lumibooks::FromBuffer(IBuffer^ b)
{
    std::vector<uint8_t> v(b->Length);
    if (!v.empty()) DataReader::FromBuffer(b)->ReadBytes(ArrayReference<unsigned char>(v.data(), (unsigned int)v.size()));
    return v;
}

static void Walk(IXmlNode^ n, std::vector<XmlElement^>& out)
{
    for (auto c = n->FirstChild; c; c = c->NextSibling)
    {
        if (c->NodeType == NodeType::ElementNode)
        {
            out.push_back(safe_cast<XmlElement^>(c));
            Walk(c, out);
        }
    }
}

static bool Is(XmlElement^ e, const wchar_t* name)
{
    const wchar_t* t = e->TagName->Data();
    const wchar_t* colon = wcschr(t, L':');
    return wcscmp(colon ? colon + 1 : t, name) == 0;
}

static std::wstring Attr(XmlElement^ e, const wchar_t* name)
{
    return e->GetAttribute(ref new String(name))->Data();
}

static std::wstring Trim(String^ s)
{
    std::wstring r = s->Data();
    size_t a = r.find_first_not_of(L" \t\r\n"), b = r.find_last_not_of(L" \t\r\n");
    return a == std::wstring::npos ? std::wstring() : r.substr(a, b - a + 1);
}

static bool IsImage(const std::string& p)
{
    size_t dot = p.rfind('.');
    if (dot == std::string::npos) return false;
    std::string ext = p.substr(dot + 1);
    for (auto& c : ext) c = (char)tolower((unsigned char)c);
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" || ext == "bmp";
}

static std::string DirOf(const std::string& path)
{
    return path.substr(0, path.rfind('/') + 1);
}

static void Target(const std::string& dir, const std::wstring& href, TocEntry& t)
{
    std::string h = Utf8(href.c_str());
    size_t hash = h.find('#');
    t.fragment = hash == std::string::npos ? std::string() : UrlDecode(h.substr(hash + 1));
    t.path = JoinPath(dir, UrlDecode(h.substr(0, hash)));
}

void Epub::ParseNav(const std::string& path)
{
    auto doc = LoadXml(path);
    if (!doc) return;
    std::vector<XmlElement^> els;
    Walk(doc, els);
    XmlElement^ nav = nullptr;
    for (auto el : els)
    {
        if (!Is(el, L"nav")) continue;
        if (Attr(el, L"epub:type").find(L"toc") != std::wstring::npos) { nav = el; break; }
        if (!nav) nav = el;
    }
    if (!nav) return;
    std::string dir = DirOf(path);
    std::function<void(IXmlNode^, int)> visit = [&](IXmlNode^ n, int depth)
    {
        for (auto c = n->FirstChild; c; c = c->NextSibling)
        {
            if (c->NodeType != NodeType::ElementNode) continue;
            auto e = safe_cast<XmlElement^>(c);
            if (Is(e, L"ol")) visit(c, depth + 1);
            else if (Is(e, L"a"))
            {
                TocEntry t;
                t.title = Trim(e->InnerText);
                t.depth = depth < 0 ? 0 : depth;
                Target(dir, Attr(e, L"href"), t);
                if (!t.title.empty()) toc.push_back(t);
            }
            else visit(c, depth);
        }
    };
    visit(nav, -1);
}

void Epub::ParseNcx(const std::string& path)
{
    auto doc = LoadXml(path);
    if (!doc) return;
    std::string dir = DirOf(path);
    std::function<void(IXmlNode^, int)> visit = [&](IXmlNode^ n, int depth)
    {
        for (auto c = n->FirstChild; c; c = c->NextSibling)
        {
            if (c->NodeType != NodeType::ElementNode) continue;
            auto e = safe_cast<XmlElement^>(c);
            if (Is(e, L"navPoint"))
            {
                TocEntry t;
                t.depth = depth;
                for (auto d = c->FirstChild; d; d = d->NextSibling)
                {
                    if (d->NodeType != NodeType::ElementNode) continue;
                    auto de = safe_cast<XmlElement^>(d);
                    if (Is(de, L"navLabel")) t.title = Trim(de->InnerText);
                    else if (Is(de, L"content")) Target(dir, Attr(de, L"src"), t);
                }
                if (!t.title.empty() && !t.path.empty()) toc.push_back(t);
                visit(c, depth + 1);
            }
            else if (!Is(e, L"navLabel") && !Is(e, L"content")) visit(c, depth);
        }
    };
    visit(doc, 0);
}

Epub::~Epub()
{
    if (open) mz_zip_reader_end(&zip);
}

bool Epub::Read(const std::string& path, std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> g(lock);
    if (!open)
    {
        auto it = files.find(path);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    int i = mz_zip_reader_locate_file(&zip, path.c_str(), nullptr, 0);
    mz_zip_archive_file_stat st;
    if (i < 0 || !mz_zip_reader_file_stat(&zip, (mz_uint)i, &st) || st.m_uncomp_size > 256u * 1024 * 1024) return false;
    out.resize((size_t)st.m_uncomp_size);
    return out.empty() || mz_zip_reader_extract_to_mem(&zip, (mz_uint)i, out.data(), out.size(), 0);
}

int Epub::SpineIndex(const std::string& path) const
{
    for (size_t i = 0; i < spine.size(); i++)
        if (_stricmp(spine[i].c_str(), path.c_str()) == 0) return (int)i;
    return -1;
}

XmlDocument^ Epub::LoadXml(const std::string& path)
{
    std::vector<uint8_t> data;
    if (!Read(path, data)) return nullptr;
    auto settings = ref new XmlLoadSettings();
    settings->ProhibitDtd = false;
    settings->ResolveExternals = false;
    auto doc = ref new XmlDocument();
    try { doc->LoadXmlFromBuffer(ToBuffer(data.data(), data.size()), settings); }
    catch (Exception^) { return nullptr; }
    return doc;
}

std::shared_ptr<Epub> Epub::Open(std::vector<uint8_t>&& data)
{
    std::shared_ptr<Epub> e(new Epub());
    e->buf = std::move(data);
    if (!mz_zip_reader_init_mem(&e->zip, e->buf.data(), e->buf.size(), 0)) return nullptr;
    e->open = true;

    auto container = e->LoadXml("META-INF/container.xml");
    if (!container) return nullptr;
    std::vector<XmlElement^> els;
    Walk(container, els);
    std::string opf;
    for (auto el : els)
        if (Is(el, L"rootfile")) { opf = Utf8(Attr(el, L"full-path").c_str()); break; }
    XmlDocument^ doc = nullptr;
    if (!opf.empty()) doc = e->LoadXml(opf);
    if (!doc) return nullptr;

    std::string dir = DirOf(opf);
    std::map<std::wstring, std::string> items;
    std::wstring coverId, tocId;
    std::string navPath, ncxPath;
    std::vector<std::wstring> refs;
    els.clear();
    Walk(doc, els);
    for (auto el : els)
    {
        if (Is(el, L"title")) { if (e->title.empty()) e->title = Trim(el->InnerText); }
        else if (Is(el, L"creator")) { if (e->author.empty()) e->author = Trim(el->InnerText); }
        else if (Is(el, L"language")) { if (e->language.empty()) e->language = Trim(el->InnerText); }
        else if (Is(el, L"meta")) { if (Attr(el, L"name") == L"cover") coverId = Attr(el, L"content"); }
        else if (Is(el, L"item"))
        {
            std::string href = JoinPath(dir, UrlDecode(Utf8(Attr(el, L"href").c_str())));
            items[Attr(el, L"id")] = href;
            std::wstring props = Attr(el, L"properties");
            if (props.find(L"cover-image") != std::wstring::npos) e->coverPath = href;
            if (props.find(L"nav") != std::wstring::npos) navPath = href;
            if (Attr(el, L"media-type") == L"application/x-dtbncx+xml") ncxPath = href;
        }
        else if (Is(el, L"spine")) tocId = Attr(el, L"toc");
        else if (Is(el, L"itemref")) refs.push_back(Attr(el, L"idref"));
    }
    for (auto& r : refs)
    {
        auto it = items.find(r);
        if (it != items.end()) e->spine.push_back(it->second);
    }
    if (e->coverPath.empty() && !coverId.empty())
    {
        auto it = items.find(coverId);
        if (it != items.end() && IsImage(it->second)) e->coverPath = it->second;
    }
    if (e->coverPath.empty())
    {
        for (auto& it : items)
        {
            std::wstring id = it.first;
            for (auto& c : id) c = towlower(c);
            if (id.find(L"cover") != std::wstring::npos && IsImage(it.second)) { e->coverPath = it.second; break; }
        }
    }
    if (e->spine.empty()) return nullptr;
    if (!tocId.empty() && items.count(tocId)) ncxPath = items[tocId];
    if (!navPath.empty()) e->ParseNav(navPath);
    if (e->toc.empty() && !ncxPath.empty()) e->ParseNcx(ncxPath);
    return e;
}

static bool StartsNoCase(const std::string& s, size_t at, const char* t)
{
    return _strnicmp(s.c_str() + at, t, strlen(t)) == 0;
}

static void PutUtf8(std::string& out, unsigned c)
{
    if (c < 0x80) out += (char)c;
    else if (c < 0x800) { out += (char)(0xc0 | c >> 6); out += (char)(0x80 | (c & 0x3f)); }
    else if (c < 0x10000) { out += (char)(0xe0 | c >> 12); out += (char)(0x80 | (c >> 6 & 0x3f)); out += (char)(0x80 | (c & 0x3f)); }
    else { out += (char)(0xf0 | c >> 18); out += (char)(0x80 | (c >> 12 & 0x3f)); out += (char)(0x80 | (c >> 6 & 0x3f)); out += (char)(0x80 | (c & 0x3f)); }
}

std::wstring Lumibooks::HtmlText(const std::vector<uint8_t>& html)
{
    std::string s(html.begin(), html.end()), out;
    size_t i = 0;
    for (size_t b = 0; (b = s.find('<', b)) != std::string::npos; b++)
        if (StartsNoCase(s, b, "<body")) { size_t g = s.find('>', b); i = g == std::string::npos ? s.size() : g + 1; break; }
    while (i < s.size())
    {
        char c = s[i];
        if (c == '<')
        {
            if (StartsNoCase(s, i, "<!--"))
            {
                size_t e = s.find("-->", i);
                i = e == std::string::npos ? s.size() : e + 3;
                continue;
            }
            const char* skip = StartsNoCase(s, i, "<script") ? "</script" : StartsNoCase(s, i, "<style") ? "</style" : nullptr;
            if (StartsNoCase(s, i, "</body")) break;
            size_t e = skip ? std::string::npos : s.find('>', i);
            if (skip)
            {
                for (size_t j = i; (j = s.find('<', j + 1)) != std::string::npos;)
                    if (StartsNoCase(s, j, skip)) { e = s.find('>', j); break; }
            }
            i = e == std::string::npos ? s.size() : e + 1;
            continue;
        }
        if (c == '&')
        {
            size_t semi = s.find(';', i);
            if (semi != std::string::npos && semi - i <= 10)
            {
                std::string name = s.substr(i + 1, semi - i - 1);
                unsigned code = 0;
                if (!name.empty() && name[0] == '#') code = (unsigned)strtoul(name.c_str() + (name.size() > 1 && (name[1] == 'x' || name[1] == 'X') ? 2 : 1), nullptr, name.size() > 1 && (name[1] == 'x' || name[1] == 'X') ? 16 : 10);
                else if (name == "amp") code = '&';
                else if (name == "lt") code = '<';
                else if (name == "gt") code = '>';
                else if (name == "quot") code = '"';
                else if (name == "apos") code = '\'';
                else if (name == "nbsp") code = 0xa0;
                if (code)
                {
                    PutUtf8(out, code);
                    i = semi + 1;
                    continue;
                }
            }
        }
        out += c;
        i++;
    }
    return Wide(out.c_str(), out.size());
}

std::vector<SearchHit> Epub::Search(const std::wstring& query, size_t max)
{
    std::vector<SearchHit> hits;
    std::wstring q = query;
    for (auto& c : q) c = towlower(c);
    if (q.empty()) return hits;
    for (int i = 0; i < (int)spine.size() && hits.size() < max; i++)
    {
        std::vector<uint8_t> data;
        if (!Read(spine[i], data)) continue;
        std::wstring text = HtmlText(data), low = text;
        for (auto& c : low) c = towlower(c);
        int k = 0;
        for (size_t at = 0; (at = low.find(q, at)) != std::wstring::npos && hits.size() < max; at++, k++)
        {
            size_t a = at > 40 ? at - 40 : 0, z = std::min(text.size(), at + q.size() + 60);
            std::wstring snip;
            bool space = false;
            for (size_t j = a; j < z; j++)
            {
                wchar_t c = text[j];
                if (iswspace(c) || c == 0xa0) { space = !snip.empty(); continue; }
                if (space) snip += L' ';
                space = false;
                snip += c;
            }
            if (a > 0) snip = L"…" + snip;
            if (z < text.size()) snip += L"…";
            hits.push_back({ i, k, snip });
        }
    }
    return hits;
}
