#include "pch.h"
#include "Settings.h"

using namespace Lumibooks;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Storage;

const wchar_t* const Settings::Fonts[] = { L"Publisher default", L"Georgia", L"Cambria", L"Times New Roman", L"Segoe UI",
    L"Calibri", L"Verdana", L"Tahoma", L"Arial", L"Trebuchet MS", L"Courier New" };
const int Settings::FontCount = 11;
const Theme Settings::Themes[] = { { 0xffffff, 0x111111 }, { 0xf4ecd8, 0x5b4636 }, { 0x1e1e1e, 0xcccccc }, { 0x000000, 0x999999 } };
const int Settings::ThemeCount = 4;
const int Settings::CustomTheme = 4;

Settings& Settings::Get()
{
    static Settings s;
    return s;
}

typedef Windows::Foundation::Collections::IPropertySet Values;

static IPropertyValue^ Find(Values^ v, String^ key, PropertyType type)
{
    if (!v->HasKey(key)) return nullptr;
    auto p = dynamic_cast<IPropertyValue^>(v->Lookup(key));
    if (!p || p->Type != type) return nullptr;
    return p;
}

static int ReadInt(Values^ v, String^ key, int def, int lo, int hi)
{
    auto p = Find(v, key, PropertyType::Int32);
    if (!p) return def;
    int x = p->GetInt32();
    return x < lo || x > hi ? def : x;
}

static bool ReadBool(Values^ v, String^ key, bool def)
{
    return ReadInt(v, key, def ? 1 : 0, 0, 1) != 0;
}

void Settings::Load()
{
    auto v = ApplicationData::Current->LocalSettings->Values;
    font = ReadInt(v, "font", font, 0, FontCount - 1);
    size = ReadInt(v, "size", size, 50, 300);
    lineHeight = ReadInt(v, "lineHeight", lineHeight, 10, 30);
    letter = ReadInt(v, "letter", letter, -5, 10);
    bold = ReadBool(v, "bold", bold);
    margin = ReadInt(v, "margin", margin, 0, 64);
    vmargin = ReadInt(v, "vmargin", vmargin, 0, 64);
    justify = ReadBool(v, "justify", justify);
    hyphen = ReadBool(v, "hyphen", hyphen);
    spacing = ReadBool(v, "spacing", spacing);
    para = ReadInt(v, "para", para, 0, 20);
    indent = ReadInt(v, "indent", indent, 0, 30);
    theme = ReadInt(v, "theme", theme, 0, CustomTheme);
    customBg = (uint32_t)ReadInt(v, "customBg", (int)customBg, 0, 0xffffff);
    customFg = (uint32_t)ReadInt(v, "customFg", (int)customFg, 0, 0xffffff);
    night = ReadBool(v, "night", night);
    warmth = ReadInt(v, "warmth", warmth, 0, 100);
    dim = ReadInt(v, "dim", dim, 0, 80);
    rate = ReadInt(v, "rate", rate, -50, 100);
    pitch = ReadInt(v, "pitch", pitch, -50, 50);
    auto p = Find(v, "voice", PropertyType::String);
    if (p) voice = p->GetString()->Data();
}

void Settings::Save() const
{
    auto v = ApplicationData::Current->LocalSettings->Values;
    auto put = [v](String^ key, int x) { v->Insert(key, PropertyValue::CreateInt32(x)); };
    put("font", font);
    put("size", size);
    put("lineHeight", lineHeight);
    put("letter", letter);
    put("bold", bold);
    put("margin", margin);
    put("vmargin", vmargin);
    put("justify", justify);
    put("hyphen", hyphen);
    put("spacing", spacing);
    put("para", para);
    put("indent", indent);
    put("theme", theme);
    put("customBg", (int)customBg);
    put("customFg", (int)customFg);
    put("night", night);
    put("warmth", warmth);
    put("dim", dim);
    put("rate", rate);
    put("pitch", pitch);
    v->Insert("voice", PropertyValue::CreateString(ref new String(voice.c_str())));
}

String^ Settings::Json() const
{
    wchar_t s[512];
    swprintf_s(s, L"{\"font\":\"%s\",\"size\":%d,\"lh\":%d.%d,\"ls\":%d,\"bold\":%s,\"margin\":%d,\"vmargin\":%d,"
        L"\"justify\":%s,\"hyph\":%s,\"para\":%d,\"indent\":%d,\"bg\":\"#%06x\",\"fg\":\"#%06x\"}",
        font ? Fonts[font] : L"", size, lineHeight / 10, lineHeight % 10, letter, bold ? L"true" : L"false", margin, vmargin,
        justify ? L"true" : L"false", hyphen ? L"true" : L"false", spacing ? para : -1, spacing ? indent : -1, Bg(), Fg());
    return ref new String(s);
}
