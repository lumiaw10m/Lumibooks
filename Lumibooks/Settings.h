#pragma once

namespace Lumibooks
{
    struct Theme { uint32_t bg, fg; };

    struct Settings
    {
        int font = 0;
        int size = 100;
        int lineHeight = 15;
        int letter = 0;
        bool bold = false;
        int margin = 16;
        int vmargin = 20;
        bool justify = true;
        bool hyphen = true;
        bool spacing = false;
        int para = 5;
        int indent = 15;
        int theme = 0;
        uint32_t customBg = 0xffffff;
        uint32_t customFg = 0x111111;
        bool night = false;
        int warmth = 40;
        int dim = 0;
        std::wstring voice;
        int rate = 0;
        int pitch = 0;

        static Settings& Get();
        static const wchar_t* const Fonts[];
        static const int FontCount;
        static const Theme Themes[];
        static const int ThemeCount;
        static const int CustomTheme;
        uint32_t Bg() const { return theme == CustomTheme ? customBg : Themes[theme].bg; }
        uint32_t Fg() const { return theme == CustomTheme ? customFg : Themes[theme].fg; }
        void Load();
        void Save() const;
        Platform::String^ Json() const;
    };
}
