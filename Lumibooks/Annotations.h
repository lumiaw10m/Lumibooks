#pragma once

namespace Lumibooks
{
    struct Bookmark
    {
        int chapter;
        double progress;
        std::wstring text, label;
    };

    struct Highlight
    {
        int id, chapter, start, end, color;
        std::wstring text, note, label;
    };

    struct Annotations
    {
        std::vector<Bookmark> bookmarks;
        std::vector<Highlight> highlights;

        void Parse(Platform::String^ json);
        Platform::String^ Json() const;
        Platform::String^ ChapterJson(int chapter) const;
        Highlight* Find(int id);
        int NextId() const;
        void Sort();
    };
}
