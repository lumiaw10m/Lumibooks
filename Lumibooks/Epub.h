#pragma once
#include "miniz.h"

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

    class Epub
    {
    public:
        ~Epub();
        static std::shared_ptr<Epub> Open(std::vector<uint8_t>&& data);
        bool Read(const std::string& path, std::vector<uint8_t>& out);
        int SpineIndex(const std::string& path) const;

        std::wstring title, author;
        std::string coverPath;
        std::vector<std::string> spine;
        std::vector<TocEntry> toc;

    private:
        Epub() = default;
        Windows::Data::Xml::Dom::XmlDocument^ LoadXml(const std::string& path);
        void ParseNav(const std::string& path);
        void ParseNcx(const std::string& path);

        std::vector<uint8_t> buf;
        mz_zip_archive zip = {};
        bool open = false;
        std::mutex lock;
    };
}
