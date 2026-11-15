#pragma once
#include "Epub.h"
#include "Mu.h"

namespace Lumibooks
{
    enum class Format { Unknown, Epub, Fb2, Mobi, Pdf, Xps, Cbz };

    Format FormatOf(const std::wstring& name);
    inline bool IsFixed(Format f) { return f == Format::Pdf || f == Format::Xps || f == Format::Cbz; }
    const wchar_t* Extension(Format f);
    extern const wchar_t* const ImportTypes[];
    extern const int ImportTypeCount;

    std::shared_ptr<Epub> OpenReflow(std::vector<uint8_t>&& data, Format f);

    struct PageEntry
    {
        std::wstring title;
        int page, depth;
    };

    class FixedDoc
    {
    public:
        ~FixedDoc();
        static std::shared_ptr<FixedDoc> Open(std::vector<uint8_t>&& data, Format f, bool* locked = nullptr);
        int Pages() const { return pages; }
        bool Size(int page, float& w, float& h);
        bool Render(int page, float scale, std::vector<uint8_t>& bgra, int& w, int& h);
        bool Cover(int width, std::vector<uint8_t>& png);

        std::wstring title, author;
        std::vector<PageEntry> toc;

    private:
        FixedDoc() = default;
        std::vector<uint8_t> buf;
        mu_doc* doc = nullptr;
        int pages = 0;
        std::mutex lock;
    };
}
