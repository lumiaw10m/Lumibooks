#pragma once

namespace Lumibooks
{
    struct Theme { const wchar_t* bg; const wchar_t* fg; Windows::UI::Color color; };

    struct Settings
    {
        int font = 0;
        int size = 100;
        int lineHeight = 15;
        int margin = 16;
        int theme = 0;
        bool justify = true;

        static Settings& Get();
        static const wchar_t* const Fonts[];
        static const int FontCount;
        static const Theme Themes[];
        static const int ThemeCount;
        void Load();
        void Save() const;
        Platform::String^ Json() const;
    };
}
