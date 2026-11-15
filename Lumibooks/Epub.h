#pragma once
#include "miniz.h"
#include <map>

namespace Lumibooks
{
    std::string Utf8(const wchar_t* s);
    std::wstring Wide(const char* s, size_t n);
    std::string UrlDecode(const std::string& s);
    std::string JoinPath(const std::string& dir, const std::string& href);
    Windows::Storage::Streams::IBuffer^ ToBuffer(const uint8_t* p, size_t n);
    std::vector<uint8_t> FromBuffer(Windows::Storage::Streams::IBuffer^ b);

    struct TocEntry
    {
        std::wstring title;
        std::string path, fragment;
        int depth;
    };

    struct SearchHit
    {
        int chapter, k;
        std::wstring snippet;
    };

    std::wstring HtmlText(const std::vector<uint8_t>& html);

    class Epub
    {
    public:
        ~Epub();
        static std::shared_ptr<Epub> Open(std::vector<uint8_t>&& data);
        static std::shared_ptr<Epub> Create() { return std::shared_ptr<Epub>(new Epub()); }
        void Add(const std::string& path, std::vector<uint8_t>&& data) { files[path] = std::move(data); }
        bool Read(const std::string& path, std::vector<uint8_t>& out);
        int SpineIndex(const std::string& path) const;
        std::vector<SearchHit> Search(const std::wstring& query, size_t max);

        std::wstring title, author, language;
        std::string coverPath;
        std::vector<std::string> spine;
        std::vector<TocEntry> toc;

    private:
        Epub() = default;
        Windows::Data::Xml::Dom::XmlDocument^ LoadXml(const std::string& path);
        void ParseNav(const std::string& path);
        void ParseNcx(const std::string& path);

        std::vector<uint8_t> buf;
        std::map<std::string, std::vector<uint8_t>> files;
        mz_zip_archive zip = {};
        bool open = false;
        std::mutex lock;
    };
}
