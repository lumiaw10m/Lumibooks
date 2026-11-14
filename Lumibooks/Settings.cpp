#include "pch.h"
#include "Settings.h"

using namespace Lumibooks;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

const wchar_t* const Settings::Fonts[] = { L"Publisher default", L"Georgia", L"Segoe UI", L"Cambria", L"Calibri", L"Verdana", L"Courier New" };
const int Settings::FontCount = 7;
const Theme Settings::Themes[] = {
    { L"#fff", L"#111", { 255, 255, 255, 255 } },
    { L"#f4ecd8", L"#5b4636", { 255, 0xf4, 0xec, 0xd8 } },
    { L"#1e1e1e", L"#ccc", { 255, 0x1e, 0x1e, 0x1e } },
    { L"#000", L"#999", { 255, 0, 0, 0 } },
};
const int Settings::ThemeCount = 4;

Settings& Settings::Get()
{
    static Settings s;
    return s;
}

static int ReadInt(Windows::Foundation::Collections::IPropertySet^ v, String^ key, int def, int lo, int hi)
{
    if (!v->HasKey(key)) return def;
    auto p = dynamic_cast<IPropertyValue^>(v->Lookup(key));
    if (!p || p->Type != PropertyType::Int32) return def;
    int x = p->GetInt32();
    return x < lo || x > hi ? def : x;
}

void Settings::Load()
{
    auto v = ApplicationData::Current->LocalSettings->Values;
    font = ReadInt(v, "font", font, 0, FontCount - 1);
    size = ReadInt(v, "size", size, 50, 300);
    lineHeight = ReadInt(v, "lineHeight", lineHeight, 10, 30);
    margin = ReadInt(v, "margin", margin, 0, 64);
    theme = ReadInt(v, "theme", theme, 0, ThemeCount - 1);
    justify = ReadInt(v, "justify", justify ? 1 : 0, 0, 1) != 0;
}

void Settings::Save() const
{
    auto v = ApplicationData::Current->LocalSettings->Values;
    v->Insert("font", PropertyValue::CreateInt32(font));
    v->Insert("size", PropertyValue::CreateInt32(size));
    v->Insert("lineHeight", PropertyValue::CreateInt32(lineHeight));
    v->Insert("margin", PropertyValue::CreateInt32(margin));
    v->Insert("theme", PropertyValue::CreateInt32(theme));
    v->Insert("justify", PropertyValue::CreateInt32(justify ? 1 : 0));
}

String^ Settings::Json() const
{
    wchar_t s[256];
    swprintf_s(s, L"{\"font\":\"%s\",\"size\":%d,\"lh\":%d.%d,\"margin\":%d,\"bg\":\"%s\",\"fg\":\"%s\",\"justify\":%s}",
        font ? Fonts[font] : L"", size, lineHeight / 10, lineHeight % 10, margin, Themes[theme].bg, Themes[theme].fg, justify ? L"true" : L"false");
    return ref new String(s);
}
