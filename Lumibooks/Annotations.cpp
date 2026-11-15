#include "pch.h"
#include "Annotations.h"

using namespace Lumibooks;
using namespace Platform;
using namespace Windows::Data::Json;

static String^ Str(const std::wstring& s)
{
    return ref new String(s.c_str(), (unsigned int)s.size());
}

void Annotations::Parse(String^ json)
{
    bookmarks.clear();
    highlights.clear();
    JsonObject^ root = nullptr;
    if (!json || !JsonObject::TryParse(json, &root)) return;
    auto marks = root->GetNamedArray("bookmarks", ref new JsonArray());
    for (unsigned int i = 0; i < marks->Size; i++)
    {
        if (marks->GetAt(i)->ValueType != JsonValueType::Object) continue;
        auto o = marks->GetObjectAt(i);
        Bookmark b;
        b.chapter = (int)o->GetNamedNumber("c", 0);
        b.progress = o->GetNamedNumber("p", 0);
        b.text = o->GetNamedString("t", "")->Data();
        b.label = o->GetNamedString("l", "")->Data();
        bookmarks.push_back(b);
    }
    auto hls = root->GetNamedArray("highlights", ref new JsonArray());
    for (unsigned int i = 0; i < hls->Size; i++)
    {
        if (hls->GetAt(i)->ValueType != JsonValueType::Object) continue;
        auto o = hls->GetObjectAt(i);
        Highlight h;
        h.id = (int)o->GetNamedNumber("id", 0);
        h.chapter = (int)o->GetNamedNumber("c", 0);
        h.start = (int)o->GetNamedNumber("s", 0);
        h.end = (int)o->GetNamedNumber("e", 0);
        h.color = (int)o->GetNamedNumber("k", 1);
        h.text = o->GetNamedString("t", "")->Data();
        h.note = o->GetNamedString("n", "")->Data();
        h.label = o->GetNamedString("l", "")->Data();
        if (h.end > h.start) highlights.push_back(h);
    }
}

String^ Annotations::Json() const
{
    auto marks = ref new JsonArray();
    for (auto& b : bookmarks)
    {
        auto o = ref new JsonObject();
        o->Insert("c", JsonValue::CreateNumberValue(b.chapter));
        o->Insert("p", JsonValue::CreateNumberValue(b.progress));
        o->Insert("t", JsonValue::CreateStringValue(Str(b.text)));
        if (!b.label.empty()) o->Insert("l", JsonValue::CreateStringValue(Str(b.label)));
        marks->Append(o);
    }
    auto hls = ref new JsonArray();
    for (auto& h : highlights)
    {
        auto o = ref new JsonObject();
        o->Insert("id", JsonValue::CreateNumberValue(h.id));
        o->Insert("c", JsonValue::CreateNumberValue(h.chapter));
        o->Insert("s", JsonValue::CreateNumberValue(h.start));
        o->Insert("e", JsonValue::CreateNumberValue(h.end));
        o->Insert("k", JsonValue::CreateNumberValue(h.color));
        o->Insert("t", JsonValue::CreateStringValue(Str(h.text)));
        o->Insert("n", JsonValue::CreateStringValue(Str(h.note)));
        if (!h.label.empty()) o->Insert("l", JsonValue::CreateStringValue(Str(h.label)));
        hls->Append(o);
    }
    auto root = ref new JsonObject();
    root->Insert("bookmarks", marks);
    root->Insert("highlights", hls);
    return root->Stringify();
}

String^ Annotations::ChapterJson(int chapter) const
{
    auto arr = ref new JsonArray();
    for (auto& h : highlights)
    {
        if (h.chapter != chapter) continue;
        auto o = ref new JsonObject();
        o->Insert("id", JsonValue::CreateNumberValue(h.id));
        o->Insert("s", JsonValue::CreateNumberValue(h.start));
        o->Insert("e", JsonValue::CreateNumberValue(h.end));
        o->Insert("c", JsonValue::CreateNumberValue(h.color));
        o->Insert("n", JsonValue::CreateBooleanValue(!h.note.empty()));
        arr->Append(o);
    }
    return arr->Stringify();
}

Highlight* Annotations::Find(int id)
{
    for (auto& h : highlights)
        if (h.id == id) return &h;
    return nullptr;
}

int Annotations::NextId() const
{
    int id = 0;
    for (auto& h : highlights)
        if (h.id > id) id = h.id;
    return id + 1;
}

void Annotations::Sort()
{
    std::stable_sort(bookmarks.begin(), bookmarks.end(), [](const Bookmark& a, const Bookmark& b)
    {
        return a.chapter != b.chapter ? a.chapter < b.chapter : a.progress < b.progress;
    });
    std::stable_sort(highlights.begin(), highlights.end(), [](const Highlight& a, const Highlight& b)
    {
        return a.chapter != b.chapter ? a.chapter < b.chapter : a.start < b.start;
    });
}
