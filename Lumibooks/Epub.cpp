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

Epub::~Epub()
{
    if (open) mz_zip_reader_end(&zip);
}

bool Epub::Read(const std::string& path, std::vector<uint8_t>& out)
{
    std::lock_guard<std::mutex> g(lock);
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

    std::string dir = opf.substr(0, opf.rfind('/') + 1);
    std::map<std::wstring, std::string> items;
    std::wstring coverId;
    std::vector<std::wstring> refs;
    els.clear();
    Walk(doc, els);
    for (auto el : els)
    {
        if (Is(el, L"title")) { if (e->title.empty()) e->title = Trim(el->InnerText); }
        else if (Is(el, L"creator")) { if (e->author.empty()) e->author = Trim(el->InnerText); }
        else if (Is(el, L"meta")) { if (Attr(el, L"name") == L"cover") coverId = Attr(el, L"content"); }
        else if (Is(el, L"item"))
        {
            std::string href = JoinPath(dir, UrlDecode(Utf8(Attr(el, L"href").c_str())));
            items[Attr(el, L"id")] = href;
            if (Attr(el, L"properties").find(L"cover-image") != std::wstring::npos) e->coverPath = href;
        }
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
    return e;
}
