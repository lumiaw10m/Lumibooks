#include "pch.h"
#include "EpubResolver.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;

static const char Inject[] = "<link rel=\"stylesheet\" href=\"/__lb/reader.css\"/><script src=\"/__lb/reader.js\"></script>";

std::string EpubResolver::PathOf(Uri^ uri)
{
    std::string p = UrlDecode(Utf8(uri->Path->Data()));
    return p.empty() || p[0] != '/' ? p : p.substr(1);
}

static size_t FindNoCase(const std::vector<uint8_t>& d, const char* s)
{
    size_t n = strlen(s);
    for (size_t i = 0; i + n <= d.size(); i++)
        if (_strnicmp((const char*)&d[i], s, n) == 0) return i;
    return std::string::npos;
}

static void AddReaderTags(std::vector<uint8_t>& d)
{
    size_t at = FindNoCase(d, "</head");
    std::string tags = Inject;
    if (at == std::string::npos)
    {
        size_t html = FindNoCase(d, "<html");
        if (html == std::string::npos) return;
        auto end = std::find(d.begin() + html, d.end(), '>');
        if (end == d.end()) return;
        at = end - d.begin() + 1;
        tags = "<head>" + tags + "</head>";
    }
    d.insert(d.begin() + at, tags.begin(), tags.end());
}

static task<IInputStream^> ToStream(IBuffer^ buf)
{
    auto ms = ref new InMemoryRandomAccessStream();
    return create_task(ms->WriteAsync(buf)).then([ms](unsigned int)
    {
        ms->Seek(0);
        return static_cast<IInputStream^>(ms);
    });
}

IAsyncOperation<IInputStream^>^ EpubResolver::UriToStreamAsync(Uri^ uri)
{
    if (!uri) throw ref new InvalidArgumentException();
    std::string path = PathOf(uri);
    auto book = epub;
    return create_async([path, book]() -> task<IInputStream^>
    {
        if (path.compare(0, 5, "__lb/") == 0)
        {
            auto name = Wide(path.c_str() + 5, path.size() - 5);
            return create_task(PathIO::ReadBufferAsync("ms-appx:///Assets/Reader/" + ref new String(name.c_str()))).then([](IBuffer^ b) { return ToStream(b); });
        }
        std::vector<uint8_t> data;
        if (!book->Read(path, data)) throw ref new COMException(HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND));
        if (book->SpineIndex(path) >= 0) AddReaderTags(data);
        return ToStream(ToBuffer(data.data(), data.size()));
    });
}
