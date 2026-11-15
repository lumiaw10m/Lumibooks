#include "pch.h"
#include "Stats.h"
#include "Library.h"
#include <ctime>
#include <cmath>
#undef GetObject

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Data::Json;

static std::map<std::wstring, BookStats> s_books;
static std::map<std::wstring, DayStats> s_days;
static std::wstring s_current;
static double s_last = 0;
static int s_unsaved = 0;
static bool s_loaded = false;
static task<void> s_load;
static const double IdleCap = 300;

double Stats::Now()
{
    return (double)_time64(nullptr);
}

std::wstring Stats::Today(int offset)
{
    __time64_t t = _time64(nullptr) + (__time64_t)offset * 86400;
    tm local;
    _localtime64_s(&local, &t);
    wchar_t s[16];
    swprintf_s(s, L"%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    return s;
}

static void Add(double seconds, int pages)
{
    if (s_current.empty()) return;
    auto& b = s_books[s_current];
    auto& d = s_days[Stats::Today()];
    b.seconds += seconds;
    b.pages += pages;
    d.seconds += seconds;
    d.pages += pages;
}

static double Elapsed()
{
    double now = Stats::Now(), dt = now - s_last;
    s_last = now;
    return dt < 0 ? 0 : dt > IdleCap ? IdleCap : dt;
}

task<void> Stats::Load()
{
    if (s_loaded) return s_load;
    s_loaded = true;
    s_load = Library::ReadText("stats.json").then([](String^ text)
    {
        JsonObject^ root = nullptr;
        if (!text || !JsonObject::TryParse(text, &root)) return;
        auto books = root->GetNamedObject("books", ref new JsonObject());
        for (auto kv : books)
        {
            if (kv->Value->ValueType != JsonValueType::Object) continue;
            auto o = kv->Value->GetObject();
            auto& b = s_books[kv->Key->Data()];
            b.seconds += o->GetNamedNumber("s", 0);
            b.pages += (int)o->GetNamedNumber("p", 0);
            b.sessions += (int)o->GetNamedNumber("n", 0);
            double first = o->GetNamedNumber("first", 0), last = o->GetNamedNumber("last", 0), fin = o->GetNamedNumber("fin", 0);
            if (first > 0 && (b.first == 0 || first < b.first)) b.first = first;
            if (last > b.last) b.last = last;
            if (fin > 0 && b.finished == 0) b.finished = fin;
        }
        auto days = root->GetNamedObject("days", ref new JsonObject());
        for (auto kv : days)
        {
            if (kv->Value->ValueType != JsonValueType::Object) continue;
            auto o = kv->Value->GetObject();
            auto& d = s_days[kv->Key->Data()];
            d.seconds += o->GetNamedNumber("s", 0);
            d.pages += (int)o->GetNamedNumber("p", 0);
        }
    }, task_continuation_context::use_current());
    return s_load;
}

void Stats::Save()
{
    s_unsaved = 0;
    Load().then([]
    {
    auto books = ref new JsonObject();
    for (auto& kv : s_books)
    {
        auto o = ref new JsonObject();
        o->Insert("s", JsonValue::CreateNumberValue(std::floor(kv.second.seconds)));
        o->Insert("p", JsonValue::CreateNumberValue(kv.second.pages));
        o->Insert("n", JsonValue::CreateNumberValue(kv.second.sessions));
        o->Insert("first", JsonValue::CreateNumberValue(kv.second.first));
        o->Insert("last", JsonValue::CreateNumberValue(kv.second.last));
        if (kv.second.finished > 0) o->Insert("fin", JsonValue::CreateNumberValue(kv.second.finished));
        books->Insert(ref new String(kv.first.c_str()), o);
    }
    auto days = ref new JsonObject();
    for (auto& kv : s_days)
    {
        auto o = ref new JsonObject();
        o->Insert("s", JsonValue::CreateNumberValue(std::floor(kv.second.seconds)));
        o->Insert("p", JsonValue::CreateNumberValue(kv.second.pages));
        days->Insert(ref new String(kv.first.c_str()), o);
    }
    auto root = ref new JsonObject();
    root->Insert("books", books);
    root->Insert("days", days);
    Library::WriteText("", "stats.json", root->Stringify());
    }, task_continuation_context::use_current());
}

void Stats::Begin(String^ id)
{
    Load();
    s_current = id->Data();
    s_last = Now();
    auto& b = s_books[s_current];
    b.sessions++;
    if (b.first == 0) b.first = s_last;
    b.last = s_last;
}

void Stats::Turn()
{
    if (s_current.empty()) return;
    Add(Elapsed(), 1);
    s_books[s_current].last = s_last;
    if (++s_unsaved >= 5) Save();
}

void Stats::End()
{
    if (s_current.empty()) return;
    Add(Elapsed(), 0);
    s_books[s_current].last = s_last;
    s_current.clear();
    Save();
}

void Stats::Finish(String^ id)
{
    auto& b = s_books[id->Data()];
    if (b.finished == 0)
    {
        b.finished = Now();
        Save();
    }
}

void Stats::Forget(String^ id)
{
    s_books.erase(id->Data());
    Save();
}

BookStats Stats::Book(String^ id)
{
    auto it = s_books.find(id->Data());
    return it == s_books.end() ? BookStats() : it->second;
}

const std::map<std::wstring, DayStats>& Stats::Days()
{
    return s_days;
}
