#include "pch.h"
#include "Formats.h"

using namespace Lumibooks;
using namespace Platform;
using namespace Windows::Data::Xml::Dom;
using namespace Windows::Security::Cryptography;

static std::wstring Lower(std::wstring s)
{
    for (auto& c : s) c = towlower(c);
    return s;
}

Format Lumibooks::FormatOf(const std::wstring& name)
{
    std::wstring n = Lower(name);
    size_t dot = n.rfind(L'.');
    if (dot == std::wstring::npos) return Format::Unknown;
    std::wstring e = n.substr(dot);
    if (e == L".epub") return Format::Epub;
    if (e == L".fb2") return Format::Fb2;
    if (e == L".mobi" || e == L".prc") return Format::Mobi;
    if (e == L".pdf") return Format::Pdf;
    if (e == L".xps" || e == L".oxps") return Format::Xps;
    if (e == L".cbz") return Format::Cbz;
    return Format::Unknown;
}

const wchar_t* Lumibooks::Extension(Format f)
{
    switch (f)
    {
    case Format::Epub: return L".epub";
    case Format::Fb2: return L".fb2";
    case Format::Mobi: return L".mobi";
    case Format::Pdf: return L".pdf";
    case Format::Xps: return L".xps";
    case Format::Cbz: return L".cbz";
    default: return L"";
    }
}

const wchar_t* const Lumibooks::ImportTypes[] = { L".epub", L".fb2", L".mobi", L".prc", L".pdf", L".xps", L".oxps", L".cbz" };
const int Lumibooks::ImportTypeCount = 8;

static std::vector<uint8_t> Bytes(const std::string& s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

static void Esc(std::string& out, const std::wstring& t)
{
    for (auto c : Utf8(t.c_str()))
    {
        if (c == '&') out += "&amp;";
        else if (c == '<') out += "&lt;";
        else if (c == '>') out += "&gt;";
        else if (c == '"') out += "&quot;";
        else out += c;
    }
}

static const char* ImageExt(const std::vector<uint8_t>& d)
{
    if (d.size() > 3 && d[0] == 0xff && d[1] == 0xd8) return ".jpg";
    if (d.size() > 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return ".png";
    if (d.size() > 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') return ".gif";
    if (d.size() > 2 && d[0] == 'B' && d[1] == 'M') return ".bmp";
    return ".jpg";
}

// ---------- FB2 ----------

namespace
{
    struct Fb2
    {
        std::shared_ptr<Epub> book;
        std::map<std::wstring, std::string> idFile;
        std::map<std::wstring, std::string> images;
        std::string file;
        int sections = 0;

        static std::wstring Name(IXmlNode^ n)
        {
            auto l = n->LocalName;
            return l ? std::wstring(safe_cast<String^>(l)->Data()) : std::wstring();
        }

        static std::wstring Get(IXmlNode^ n, const wchar_t* local)
        {
            if (!n->Attributes) return std::wstring();
            for (auto a : n->Attributes)
                if (Name(a) == local) return safe_cast<String^>(a->NodeValue)->Data();
            return std::wstring();
        }

        static IXmlNode^ Child(IXmlNode^ n, const wchar_t* name)
        {
            for (auto c = n ? n->FirstChild : nullptr; c; c = c->NextSibling)
                if (c->NodeType == NodeType::ElementNode && Name(c) == name) return c;
            return nullptr;
        }

        static std::wstring Text(IXmlNode^ n)
        {
            if (!n) return std::wstring();
            std::wstring r = safe_cast<String^>(n->InnerText)->Data();
            std::wstring o;
            bool space = false;
            for (auto c : r)
            {
                if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n') { space = !o.empty(); continue; }
                if (space) o += L' ';
                space = false;
                o += c;
            }
            return o;
        }

        void Ids(IXmlNode^ n)
        {
            for (auto c = n->FirstChild; c; c = c->NextSibling)
            {
                if (c->NodeType != NodeType::ElementNode) continue;
                auto id = Get(c, L"id");
                if (!id.empty()) idFile[id] = file;
                Ids(c);
            }
        }

        std::string Href(const std::wstring& h)
        {
            if (h.empty() || h[0] != L'#') return Utf8(h.c_str());
            auto it = idFile.find(h.substr(1));
            return (it == idFile.end() ? std::string() : it->second) + Utf8(h.c_str());
        }

        std::string Image(IXmlNode^ n)
        {
            auto h = Get(n, L"href");
            if (h.empty() || h[0] != L'#') return std::string();
            auto it = images.find(h.substr(1));
            return it == images.end() ? std::string() : it->second;
        }

        void Inline(std::string& out, IXmlNode^ n)
        {
            for (auto c = n->FirstChild; c; c = c->NextSibling)
            {
                if (c->NodeType == NodeType::TextNode || c->NodeType == NodeType::DataSectionNode)
                {
                    Esc(out, safe_cast<String^>(c->NodeValue)->Data());
                    continue;
                }
                if (c->NodeType != NodeType::ElementNode) continue;
                auto nm = Name(c);
                const char* tag = nm == L"strong" ? "strong" : nm == L"emphasis" ? "em" : nm == L"strikethrough" ? "s"
                    : nm == L"sub" ? "sub" : nm == L"sup" ? "sup" : nm == L"code" ? "code" : nullptr;
                if (tag)
                {
                    out += std::string("<") + tag + ">";
                    Inline(out, c);
                    out += std::string("</") + tag + ">";
                }
                else if (nm == L"a")
                {
                    bool note = Get(c, L"type") == L"note";
                    out += note ? "<sup><a href=\"" : "<a href=\"";
                    std::wstring h = Get(c, L"href");
                    std::string href = Href(h);
                    std::string esc;
                    Esc(esc, Wide(href.c_str(), href.size()));
                    out += esc + "\">";
                    Inline(out, c);
                    out += note ? "</a></sup>" : "</a>";
                }
                else if (nm == L"image")
                {
                    auto src = Image(c);
                    if (!src.empty()) out += "<img src=\"" + src + "\" alt=\"\"/>";
                }
                else Inline(out, c);
            }
        }

        std::string Id(IXmlNode^ n)
        {
            auto id = Get(n, L"id");
            if (id.empty()) return std::string();
            std::string r = " id=\"";
            Esc(r, id);
            return r + "\"";
        }

        void Block(std::string& out, IXmlNode^ n, int depth)
        {
            for (auto c = n->FirstChild; c; c = c->NextSibling) Node(out, c, depth);
        }

        void Node(std::string& out, IXmlNode^ c, int depth)
        {
            {
                if (c->NodeType != NodeType::ElementNode) return;
                auto nm = Name(c);
                if (nm == L"p") { out += "<p" + Id(c) + ">"; Inline(out, c); out += "</p>"; }
                else if (nm == L"v") { out += "<p class=\"fb-v\"" + Id(c) + ">"; Inline(out, c); out += "</p>"; }
                else if (nm == L"subtitle") { out += "<p class=\"fb-subtitle\"" + Id(c) + "><b>"; Inline(out, c); out += "</b></p>"; }
                else if (nm == L"text-author") { out += "<p class=\"fb-author\">"; Inline(out, c); out += "</p>"; }
                else if (nm == L"empty-line") out += "<p class=\"fb-empty\">&#160;</p>";
                else if (nm == L"image")
                {
                    auto src = Image(c);
                    if (!src.empty()) out += "<div class=\"fb-img\"" + Id(c) + "><img src=\"" + src + "\" alt=\"\"/></div>";
                }
                else if (nm == L"title")
                {
                    int h = depth < 1 ? 1 : depth > 6 ? 6 : depth;
                    out += "<h" + std::to_string(h) + " class=\"fb-title\">";
                    bool first = true;
                    for (auto p = c->FirstChild; p; p = p->NextSibling)
                    {
                        if (p->NodeType != NodeType::ElementNode || Name(p) != L"p") continue;
                        if (!first) out += "<br/>";
                        Inline(out, p);
                        first = false;
                    }
                    out += "</h" + std::to_string(h) + ">";
                }
                else if (nm == L"epigraph" || nm == L"cite" || nm == L"annotation")
                {
                    out += "<blockquote class=\"fb-" + Utf8(nm.c_str()) + "\"" + Id(c) + ">";
                    Block(out, c, depth);
                    out += "</blockquote>";
                }
                else if (nm == L"poem" || nm == L"stanza")
                {
                    out += "<div class=\"fb-" + Utf8(nm.c_str()) + "\"" + Id(c) + ">";
                    Block(out, c, depth + 1);
                    out += "</div>";
                }
                else if (nm == L"section")
                {
                    std::string id = Id(c);
                    if (id.empty()) id = " id=\"s" + std::to_string(++sections) + "\"";
                    auto title = Text(Child(c, L"title"));
                    if (!title.empty())
                    {
                        TocEntry t;
                        t.title = title;
                        t.path = file;
                        t.fragment = id.substr(5, id.size() - 6);
                        t.depth = depth - 1;
                        book->toc.push_back(t);
                    }
                    out += "<div class=\"fb-section\"" + id + ">";
                    Block(out, c, depth + 1);
                    out += "</div>";
                }
                else if (nm == L"table" || nm == L"tr" || nm == L"td" || nm == L"th")
                {
                    std::string tag = Utf8(nm.c_str());
                    out += "<" + tag + Id(c) + ">";
                    if (nm == L"td" || nm == L"th") Inline(out, c); else Block(out, c, depth);
                    out += "</" + tag + ">";
                }
            }
        }

        void Chapter(const std::string& name, const std::wstring& title, const std::string& body)
        {
            std::string x = "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>";
            Esc(x, title);
            x += "</title><link rel=\"stylesheet\" href=\"fb2.css\"/></head><body>" + body + "</body></html>";
            book->Add(name, Bytes(x));
            book->spine.push_back(name);
        }
    };

    UINT CodePage(const std::string& head)
    {
        std::string h = head;
        for (auto& c : h) c = (char)tolower((unsigned char)c);
        size_t e = h.find("encoding");
        if (e == std::string::npos || h.find("?>") < e) return CP_UTF8;
        size_t q = h.find_first_of("\"'", e);
        if (q == std::string::npos) return CP_UTF8;
        size_t z = h.find(h[q], q + 1);
        std::string enc = h.substr(q + 1, z == std::string::npos ? 0 : z - q - 1);
        if (enc == "windows-1251" || enc == "cp1251") return 1251;
        if (enc == "windows-1252" || enc == "cp1252") return 1252;
        if (enc == "windows-1250") return 1250;
        if (enc == "koi8-r") return 20866;
        if (enc == "koi8-u") return 21866;
        if (enc == "iso-8859-1" || enc == "latin1") return 28591;
        if (enc == "iso-8859-2") return 28592;
        if (enc == "iso-8859-5") return 28595;
        if (enc == "utf-16") return 1200;
        return CP_UTF8;
    }
}

static std::shared_ptr<Epub> ConvertFb2(const std::vector<uint8_t>& data)
{
    if (data.size() < 16) return nullptr;
    size_t start = data.size() >= 3 && data[0] == 0xef && data[1] == 0xbb && data[2] == 0xbf ? 3 : 0;
    std::string head((const char*)data.data() + start, std::min<size_t>(200, data.size() - start));
    UINT cp = CodePage(head);
    std::wstring text;
    if (cp == 1200) text.assign((const wchar_t*)data.data(), data.size() / 2);
    else
    {
        int n = MultiByteToWideChar(cp, 0, (const char*)data.data() + start, (int)(data.size() - start), nullptr, 0);
        if (n <= 0) return nullptr;
        text.resize(n);
        MultiByteToWideChar(cp, 0, (const char*)data.data() + start, (int)(data.size() - start), &text[0], n);
    }
    if (!text.empty() && text[0] == 0xfeff) text.erase(0, 1);
    if (text.compare(0, 5, L"<?xml") == 0)
    {
        size_t e = text.find(L"?>");
        if (e != std::wstring::npos) text.erase(0, e + 2);
    }
    auto doc = ref new XmlDocument();
    auto settings = ref new XmlLoadSettings();
    settings->ProhibitDtd = false;
    settings->ResolveExternals = false;
    try { doc->LoadXml(ref new String(text.c_str(), (unsigned int)text.size()), settings); }
    catch (Exception^) { return nullptr; }
    auto root = doc->DocumentElement;
    if (!root || Fb2::Name(root) != L"FictionBook") return nullptr;

    Fb2 f;
    f.book = Epub::Create();
    auto& book = *f.book;
    int imageCount = 0;
    for (auto c = root->FirstChild; c; c = c->NextSibling)
    {
        if (c->NodeType != NodeType::ElementNode || Fb2::Name(c) != L"binary") continue;
        auto id = Fb2::Get(c, L"id");
        if (id.empty()) continue;
        try
        {
            auto buf = CryptographicBuffer::DecodeFromBase64String(c->InnerText);
            auto bytes = FromBuffer(buf);
            std::string name = "img/i" + std::to_string(++imageCount) + ImageExt(bytes);
            f.images[id] = name;
            book.Add(name, std::move(bytes));
        }
        catch (Exception^) {}
    }

    auto info = Fb2::Child(Fb2::Child(root, L"description"), L"title-info");
    book.title = Fb2::Text(Fb2::Child(info, L"book-title"));
    if (auto a = Fb2::Child(info, L"author"))
    {
        std::wstring name;
        for (auto part : { L"first-name", L"middle-name", L"last-name" })
        {
            auto t = Fb2::Text(Fb2::Child(a, part));
            if (t.empty()) continue;
            if (!name.empty()) name += L' ';
            name += t;
        }
        book.author = name.empty() ? Fb2::Text(Fb2::Child(a, L"nickname")) : name;
    }
    if (auto cp = Fb2::Child(info, L"coverpage"))
        if (auto img = Fb2::Child(cp, L"image")) book.coverPath = f.Image(img);

    struct Part { IXmlNode^ node; std::string file; std::wstring title; int kind; };
    std::vector<Part> parts;
    int n = 0;
    auto next = [&n]() { return "c" + std::to_string(++n) + ".xhtml"; };
    for (auto b = root->FirstChild; b; b = b->NextSibling)
    {
        if (b->NodeType != NodeType::ElementNode || Fb2::Name(b) != L"body") continue;
        auto kind = Fb2::Get(b, L"name");
        std::wstring title = Fb2::Text(Fb2::Child(b, L"title"));
        if (!kind.empty() || !Fb2::Child(b, L"section"))
        {
            parts.push_back({ b, next(), title.empty() ? (kind == L"notes" ? std::wstring(L"Notes") : kind) : title, 0 });
            continue;
        }
        bool lead = false;
        for (auto s = b->FirstChild; s && !lead; s = s->NextSibling)
        {
            if (s->NodeType != NodeType::ElementNode) continue;
            if (Fb2::Name(s) == L"section") break;
            lead = true;
        }
        if (lead) parts.push_back({ b, next(), std::wstring(), 1 });
        for (auto s = b->FirstChild; s; s = s->NextSibling)
            if (s->NodeType == NodeType::ElementNode && Fb2::Name(s) == L"section")
                parts.push_back({ s, next(), Fb2::Text(Fb2::Child(s, L"title")), 2 });
    }
    if (parts.empty()) return nullptr;

    for (auto& p : parts)
    {
        f.file = p.file;
        auto id = Fb2::Get(p.node, L"id");
        if (!id.empty()) f.idFile[id] = p.file;
        if (p.kind == 1)
        {
            for (auto s = p.node->FirstChild; s; s = s->NextSibling)
            {
                if (s->NodeType != NodeType::ElementNode) continue;
                if (Fb2::Name(s) == L"section") break;
                auto sid = Fb2::Get(s, L"id");
                if (!sid.empty()) f.idFile[sid] = p.file;
                f.Ids(s);
            }
        }
        else f.Ids(p.node);
    }
    for (auto& p : parts)
    {
        f.file = p.file;
        if (!p.title.empty())
        {
            TocEntry t;
            t.title = p.title;
            t.path = p.file;
            t.depth = 0;
            book.toc.push_back(t);
        }
        std::string body;
        if (p.kind == 1)
        {
            for (auto s = p.node->FirstChild; s; s = s->NextSibling)
            {
                if (s->NodeType == NodeType::ElementNode && Fb2::Name(s) == L"section") break;
                f.Node(body, s, 1);
            }
        }
        else if (p.kind == 0) f.Block(body, p.node, 1);
        else
        {
            body = "<div class=\"fb-section\"" + f.Id(p.node) + ">";
            f.Block(body, p.node, 1);
            body += "</div>";
        }
        f.Chapter(p.file, p.title.empty() ? book.title : p.title, body);
    }
    std::string css = "body{font-family:serif}.fb-title{text-align:center;margin:1em 0}.fb-subtitle{text-align:center}"
        ".fb-epigraph{font-style:italic;margin:1em 0 1em 25%}.fb-author{text-align:right;font-style:italic}"
        ".fb-poem{margin:1em 2em}.fb-stanza{margin:0 0 1em}.fb-v{margin:0;text-indent:0}.fb-img{text-align:center;margin:1em 0}";
    book.Add("fb2.css", Bytes(css));
    if (book.title.empty()) book.title = L"Untitled";
    return book.spine.empty() ? nullptr : f.book;
}

// ---------- MOBI ----------

static uint32_t BE32(const uint8_t* p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint16_t BE16(const uint8_t* p) { return (uint16_t)(p[0] << 8 | p[1]); }

static std::wstring Latin(const uint8_t* p, size_t n, bool utf8)
{
    if (utf8) return Wide((const char*)p, n);
    std::wstring r;
    for (size_t i = 0; i < n; i++) r += (wchar_t)p[i];
    return r;
}

static void MobiMeta(const std::vector<uint8_t>& d, Epub& book, int& cover)
{
    cover = -1;
    if (d.size() < 78 + 8) return;
    unsigned n = BE16(&d[76]);
    if (d.size() < 78 + n * 8u || !n) return;
    uint32_t r0 = BE32(&d[78]);
    if (r0 + 0x84 > d.size() || memcmp(&d[r0 + 16], "MOBI", 4) != 0) return;
    const uint8_t* m = &d[r0];
    uint32_t hlen = BE32(m + 20), enc = BE32(m + 28), firstImage = BE32(m + 0x6c);
    bool utf8 = enc == 65001;
    uint32_t nameOff = BE32(m + 0x54), nameLen = BE32(m + 0x58);
    if (r0 + nameOff + nameLen <= d.size()) book.title = Latin(m + nameOff, nameLen, utf8);
    if (!(BE32(m + 0x80) & 0x40)) return;
    size_t x = r0 + 16 + hlen;
    if (x + 12 > d.size() || memcmp(&d[x], "EXTH", 4) != 0) return;
    uint32_t count = BE32(&d[x + 8]);
    size_t p = x + 12;
    (void)firstImage;
    for (uint32_t i = 0; i < count && p + 8 <= d.size(); i++)
    {
        uint32_t type = BE32(&d[p]), len = BE32(&d[p + 4]);
        if (len < 8 || p + len > d.size()) break;
        const uint8_t* v = &d[p + 8];
        size_t vl = len - 8;
        if (type == 100 && book.author.empty()) book.author = Latin(v, vl, utf8);
        else if (type == 503) book.title = Latin(v, vl, utf8);
        else if (type == 201 && vl >= 4) cover = (int)BE32(v);
        p += len;
    }
}

static size_t FindNoCase(const std::string& s, const char* t, size_t from)
{
    size_t n = strlen(t);
    for (size_t i = from; i + n <= s.size(); i++)
        if (_strnicmp(s.c_str() + i, t, n) == 0) return i;
    return std::string::npos;
}

static std::wstring HeadingOf(const std::string& html)
{
    for (size_t p = 0; (p = FindNoCase(html, "<h", p)) != std::string::npos; p += 2)
    {
        char level = p + 2 < html.size() ? html[p + 2] : 0;
        if (level < '1' || level > '3') continue;
        size_t gt = html.find('>', p);
        char close[5] = { '<', '/', 'h', level, 0 };
        size_t end = gt == std::string::npos ? gt : FindNoCase(html, close, gt);
        if (end == std::string::npos) return std::wstring();
        std::string t;
        bool tag = false;
        for (size_t i = gt + 1; i < end; i++)
        {
            char c = html[i];
            if (c == '<') tag = true;
            else if (c == '>') tag = false;
            else if (!tag) t += isspace((unsigned char)c) ? ' ' : c;
        }
        for (auto e : { std::make_pair("&amp;", "&"), std::make_pair("&nbsp;", " "), std::make_pair("&lt;", "<"), std::make_pair("&gt;", ">") })
            for (size_t q; (q = t.find(e.first)) != std::string::npos;) t.replace(q, strlen(e.first), e.second);
        size_t a = t.find_first_not_of(' '), z = t.find_last_not_of(' ');
        if (a == std::string::npos || z - a > 200) return std::wstring();
        return Wide(t.c_str() + a, z - a + 1);
    }
    return std::wstring();
}

static std::shared_ptr<Epub> ConvertMobi(const std::vector<uint8_t>& data)
{
    std::map<std::string, std::vector<uint8_t>> entries;
    auto collect = [](void* user, const char* name, const unsigned char* p, size_t n)
    {
        try { (*(std::map<std::string, std::vector<uint8_t>>*)user)[name].assign(p, p + n); }
        catch (...) {}
    };
    if (!mu_mobi(data.data(), data.size(), collect, &entries)) return nullptr;
    auto it = entries.find("index.html");
    if (it == entries.end()) return nullptr;
    auto book = Epub::Create();
    int cover = -1;
    MobiMeta(data, *book, cover);

    std::map<std::string, std::string> names;
    for (auto& e : entries)
    {
        if (e.first == "index.html") continue;
        std::string name = "img/" + e.first + ImageExt(e.second);
        names[e.first] = name;
        if (cover >= 0 && atoi(e.first.c_str()) == cover + 1) book->coverPath = name;
        book->Add(name, std::move(e.second));
    }
    std::string html((const char*)it->second.data(), it->second.size());

    std::vector<size_t> targets;
    for (size_t p = 0; (p = FindNoCase(html, "filepos=", p)) != std::string::npos; p += 8)
    {
        size_t q = p + 8;
        while (q < html.size() && (html[q] == '"' || html[q] == '\'')) q++;
        targets.push_back((size_t)strtoul(html.c_str() + q, nullptr, 10));
    }
    std::sort(targets.begin(), targets.end());
    targets.erase(std::unique(targets.begin(), targets.end()), targets.end());
    for (auto t = targets.rbegin(); t != targets.rend(); ++t)
    {
        if (*t >= html.size()) continue;
        size_t at = *t;
        if (html[at] != '<')
        {
            size_t lt = html.rfind('<', at), gt = html.rfind('>', at);
            if (lt != std::string::npos && (gt == std::string::npos || lt > gt)) at = lt;
        }
        html.insert(at, "<a id=\"fp" + std::to_string(*t) + "\"></a>");
    }
    std::string out;
    for (size_t p = 0;;)
    {
        size_t a = FindNoCase(html, "filepos=", p);
        size_t r = FindNoCase(html, "recindex=", p);
        size_t at = std::min(a, r);
        if (at == std::string::npos) { out.append(html, p, std::string::npos); break; }
        out.append(html, p, at - p);
        bool isLink = at == a;
        size_t q = at + (isLink ? 8 : 9);
        char quote = q < html.size() && (html[q] == '"' || html[q] == '\'') ? html[q] : 0;
        if (quote) q++;
        size_t e = q;
        while (e < html.size() && isdigit((unsigned char)html[e])) e++;
        std::string num = html.substr(q, e - q);
        if (quote && e < html.size() && html[e] == quote) e++;
        if (isLink) out += "href=\"#fp" + std::to_string(strtoul(num.c_str(), nullptr, 10)) + "\"";
        else
        {
            char key[16];
            sprintf_s(key, "%05d", atoi(num.c_str()));
            auto n = names.find(key);
            out += "src=\"" + (n == names.end() ? std::string() : n->second) + "\"";
        }
        p = e;
    }

    size_t bodyAt = FindNoCase(out, "<body", 0);
    size_t bodyEnd = bodyAt == std::string::npos ? std::string::npos : out.find('>', bodyAt);
    std::string head = "<html><head><meta charset=\"utf-8\"/></head>";
    size_t start = 0;
    if (bodyEnd != std::string::npos)
    {
        head = out.substr(0, bodyAt);
        size_t h = FindNoCase(head, "<head", 0);
        if (h != std::string::npos)
        {
            size_t g = head.find('>', h);
            if (g != std::string::npos) head.insert(g + 1, "<meta charset=\"utf-8\"/>");
        }
        else head += "<head><meta charset=\"utf-8\"/></head>";
        start = bodyEnd + 1;
    }
    size_t end = FindNoCase(out, "</body", start);
    if (end == std::string::npos) end = out.size();
    std::vector<std::string> chunks;
    for (size_t p = start;;)
    {
        size_t b = FindNoCase(out, "<mbp:pagebreak", p);
        if (b == std::string::npos || b > end) { chunks.push_back(out.substr(p, end - p)); break; }
        chunks.push_back(out.substr(p, b - p));
        size_t g = out.find('>', b);
        p = g == std::string::npos ? end : g + 1;
    }
    std::map<std::string, std::string> anchorFile;
    std::vector<std::string> files;
    for (auto& c : chunks)
    {
        bool text = false;
        for (size_t i = 0; i < c.size() && !text; i++)
            if (c[i] == '<') { size_t g = c.find('>', i); if (g == std::string::npos) break; if (FindNoCase(c, "<img", i) == i) text = true; i = g; }
            else if (!isspace((unsigned char)c[i])) text = true;
        if (!text && !files.empty()) { c.clear(); continue; }
        std::string name = "c" + std::to_string(files.size() + 1) + ".html";
        files.push_back(name);
        for (size_t p = 0; (p = c.find("<a id=\"fp", p)) != std::string::npos; p += 9)
        {
            size_t z = c.find('"', p + 9);
            if (z != std::string::npos) anchorFile[c.substr(p + 7, z - p - 7)] = name;
        }
    }
    size_t k = 0;
    for (auto& c : chunks)
    {
        if (c.empty() && k < files.size() && k > 0) continue;
        if (k >= files.size()) break;
        std::string body;
        for (size_t p = 0;;)
        {
            size_t h = c.find("href=\"#fp", p);
            if (h == std::string::npos) { body.append(c, p, std::string::npos); break; }
            size_t z = c.find('"', h + 7);
            if (z == std::string::npos) { body.append(c, p, std::string::npos); break; }
            std::string id = c.substr(h + 7, z - h - 7);
            auto f = anchorFile.find(id);
            body.append(c, p, h - p);
            body += "href=\"" + (f == anchorFile.end() ? std::string() : f->second) + "#" + id + "\"";
            p = z + 1;
        }
        auto title = HeadingOf(body);
        if (!title.empty())
        {
            TocEntry t;
            t.title = title;
            t.path = files[k];
            t.depth = 0;
            book->toc.push_back(t);
        }
        book->Add(files[k], Bytes(head + "<body>" + body + "</body></html>"));
        book->spine.push_back(files[k]);
        k++;
    }
    if (book->spine.empty()) return nullptr;
    if (book->title.empty()) book->title = L"Untitled";
    return book;
}

std::shared_ptr<Epub> Lumibooks::OpenReflow(std::vector<uint8_t>&& data, Format f)
{
    if (f == Format::Epub) return Epub::Open(std::move(data));
    if (f == Format::Fb2) return ConvertFb2(data);
    if (f == Format::Mobi) return ConvertMobi(data);
    return nullptr;
}

// ---------- MuPDF documents ----------

FixedDoc::~FixedDoc()
{
    mu_close(doc);
}

std::shared_ptr<FixedDoc> FixedDoc::Open(std::vector<uint8_t>&& data, Format f, bool* locked)
{
    std::shared_ptr<FixedDoc> d(new FixedDoc());
    d->buf = std::move(data);
    const char* magic = f == Format::Pdf ? "pdf" : f == Format::Xps ? "xps" : "cbz";
    int err = MU_FAILED;
    d->doc = mu_open(d->buf.data(), d->buf.size(), magic, &err);
    if (locked) *locked = err == MU_PASSWORD;
    if (!d->doc || err != MU_OK) return nullptr;
    d->pages = mu_pages(d->doc);
    if (d->pages <= 0) return nullptr;
    char s[512];
    if (mu_meta(d->doc, "info:Title", s, sizeof s)) d->title = Wide(s, strlen(s));
    if (mu_meta(d->doc, "info:Author", s, sizeof s)) d->author = Wide(s, strlen(s));
    auto add = [](void* user, const char* title, int page, int depth)
    {
        try
        {
            if (page < 0) return;
            PageEntry e;
            e.title = Wide(title, strlen(title));
            e.page = page;
            e.depth = depth;
            ((std::vector<PageEntry>*)user)->push_back(e);
        }
        catch (...) {}
    };
    mu_outline(d->doc, add, &d->toc);
    return d;
}

bool FixedDoc::Size(int page, float& w, float& h)
{
    std::lock_guard<std::mutex> g(lock);
    return mu_page_size(doc, page, &w, &h) != 0;
}

bool FixedDoc::Render(int page, float scale, std::vector<uint8_t>& bgra, int& w, int& h)
{
    std::lock_guard<std::mutex> g(lock);
    unsigned char* p = mu_render(doc, page, scale, &w, &h);
    if (!p) return false;
    bgra.assign(p, p + (size_t)w * h * 4);
    mu_free(p);
    return true;
}

bool FixedDoc::Cover(int width, std::vector<uint8_t>& png)
{
    std::lock_guard<std::mutex> g(lock);
    size_t n = 0;
    unsigned char* p = mu_render_png(doc, 0, width, &n);
    if (!p) return false;
    png.assign(p, p + n);
    mu_free(p);
    return true;
}
