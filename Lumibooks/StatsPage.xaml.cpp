#include "pch.h"
#include "StatsPage.xaml.h"
#include "FixedReaderPage.xaml.h"
#include "Stats.h"
#include <ctime>
#include <cmath>

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Shapes;

static std::wstring Span(double s)
{
    int m = (int)(s / 60 + 0.5);
    wchar_t b[32];
    if (m < 1) return L"<1 min";
    if (m < 60) swprintf_s(b, L"%d min", m);
    else swprintf_s(b, L"%d h %02d min", m / 60, m % 60);
    return b;
}

static std::wstring Date(double t)
{
    if (t <= 0) return L"";
    __time64_t v = (__time64_t)t;
    tm local;
    _localtime64_s(&local, &v);
    wchar_t b[32];
    wcsftime(b, 32, L"%d %b %Y", &local);
    return b[0] == L'0' ? b + 1 : b;
}

static std::wstring Num(int n)
{
    wchar_t b[16];
    swprintf_s(b, L"%d", n);
    return b;
}

static String^ Str(const std::wstring& s)
{
    return ref new String(s.c_str(), (unsigned int)s.size());
}

static std::wstring Speed(const BookStats& s)
{
    if (s.seconds < 300 || s.pages < 1) return L"";
    return Num((int)(s.pages * 3600.0 / s.seconds + 0.5)) + L" pages/h";
}

static std::wstring Left(Book^ b, const BookStats& s)
{
    if (s.seconds < 300 || b->Percent < 2 || b->Percent >= 99.5) return L"";
    return L"~" + Span(s.seconds * (100 - b->Percent) / b->Percent) + L" left";
}

static std::wstring Join(std::initializer_list<std::wstring> parts)
{
    std::wstring r;
    for (auto& p : parts)
    {
        if (p.empty()) continue;
        if (!r.empty()) r += L"  ·  ";
        r += p;
    }
    return r;
}

StatsPage::StatsPage()
{
    InitializeComponent();
}

void StatsPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    StatsPage^ self = this;
    Stats::Load().then([self] { self->Fill(); }, task_continuation_context::use_current());
}

static Border^ Tile(const std::wstring& value, const std::wstring& caption)
{
    auto big = ref new TextBlock();
    big->Text = Str(value);
    big->FontSize = 26;
    big->TextTrimming = Windows::UI::Xaml::TextTrimming::CharacterEllipsis;
    auto caption2 = ref new TextBlock();
    caption2->Text = Str(caption);
    caption2->Opacity = 0.7;
    caption2->FontSize = 13;
    caption2->TextWrapping = Windows::UI::Xaml::TextWrapping::Wrap;
    auto panel = ref new StackPanel();
    panel->Children->Append(big);
    panel->Children->Append(caption2);
    auto tile = ref new Border();
    tile->Background = safe_cast<Brush^>(Application::Current->Resources->Lookup("SystemControlBackgroundBaseLowBrush"));
    tile->Padding = Thickness(12, 8, 12, 10);
    tile->Margin = Thickness(4);
    tile->Child = panel;
    return tile;
}

void StatsPage::Fill()
{
    auto& days = Stats::Days();
    double total = 0, week = 0, today = 0;
    int pages = 0, readingDays = 0;
    auto todayKey = Stats::Today();
    std::vector<std::wstring> lastWeek;
    for (int i = 0; i < 7; i++) lastWeek.push_back(Stats::Today(-i));
    for (auto& kv : days)
    {
        total += kv.second.seconds;
        pages += kv.second.pages;
        if (kv.second.seconds >= 60) readingDays++;
        if (kv.first == todayKey) today = kv.second.seconds;
        if (std::find(lastWeek.begin(), lastWeek.end(), kv.first) != lastWeek.end()) week += kv.second.seconds;
    }
    auto read = [&days](int offset)
    {
        auto it = days.find(Stats::Today(offset));
        return it != days.end() && it->second.seconds >= 60;
    };
    int streak = 0, start = read(0) ? 0 : -1;
    while (read(start - streak)) streak++;
    int longest = 0, run = 0;
    std::wstring prev;
    for (auto& kv : days)
    {
        if (kv.second.seconds < 60) { run = 0; continue; }
        tm t = {};
        swscanf_s(kv.first.c_str(), L"%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday);
        t.tm_year -= 1900;
        t.tm_mon -= 1;
        t.tm_hour = 12;
        __time64_t at = _mktime64(&t);
        tm y;
        __time64_t before = at - 86400;
        _localtime64_s(&y, &before);
        wchar_t key[16];
        swprintf_s(key, L"%04d-%02d-%02d", y.tm_year + 1900, y.tm_mon + 1, y.tm_mday);
        run = prev == key ? run + 1 : 1;
        prev = kv.first;
        longest = std::max(longest, run);
    }
    int finished = 0;
    for (auto b : Library::Books())
        if (Stats::Book(b->Id).finished > 0) finished++;

    Tiles->Children->Clear();
    Tiles->ColumnDefinitions->Clear();
    Tiles->RowDefinitions->Clear();
    for (int i = 0; i < 2; i++) Tiles->ColumnDefinitions->Append(ref new ColumnDefinition());
    for (int i = 0; i < 3; i++)
    {
        auto r = ref new RowDefinition();
        r->Height = GridLengthHelper::Auto;
        Tiles->RowDefinitions->Append(r);
    }
    Border^ tiles[] = {
        Tile(Span(today), L"Today"),
        Tile(Span(week), L"Last 7 days"),
        Tile(Span(total), L"All time"),
        Tile(Num(pages), L"Pages turned"),
        Tile(Num(finished), finished == 1 ? L"Book finished" : L"Books finished"),
        Tile(Num(streak) + (streak == 1 ? L" day" : L" days"), L"Streak, best " + Num(longest)) };
    for (int i = 0; i < 6; i++)
    {
        Grid::SetColumn(tiles[i], i % 2);
        Grid::SetRow(tiles[i], i / 2);
        Tiles->Children->Append(tiles[i]);
    }

    Chart->Children->Clear();
    Chart->ColumnDefinitions->Clear();
    ChartLabels->Children->Clear();
    ChartLabels->ColumnDefinitions->Clear();
    double most = 0, sum = 0;
    int active = 0;
    std::vector<double> minutes;
    for (int i = -13; i <= 0; i++)
    {
        auto it = days.find(Stats::Today(i));
        double m = it == days.end() ? 0 : it->second.seconds / 60;
        minutes.push_back(m);
        most = std::max(most, m);
        if (m >= 1) { sum += m; active++; }
    }
    auto accent = safe_cast<Brush^>(Application::Current->Resources->Lookup("SystemControlBackgroundAccentBrush"));
    auto faint = safe_cast<Brush^>(Application::Current->Resources->Lookup("SystemControlBackgroundBaseLowBrush"));
    for (int i = 0; i < 14; i++)
    {
        Chart->ColumnDefinitions->Append(ref new ColumnDefinition());
        ChartLabels->ColumnDefinitions->Append(ref new ColumnDefinition());
        auto bar = ref new Rectangle();
        bar->Fill = minutes[i] >= 1 ? accent : faint;
        bar->Margin = Thickness(2, 0, 2, 0);
        bar->VerticalAlignment = Windows::UI::Xaml::VerticalAlignment::Bottom;
        bar->Height = most > 0 && minutes[i] >= 1 ? std::max(4.0, 140 * minutes[i] / most) : 2;
        Grid::SetColumn(bar, i);
        Chart->Children->Append(bar);
        __time64_t t = _time64(nullptr) + (__time64_t)(i - 13) * 86400;
        tm local;
        _localtime64_s(&local, &t);
        auto label = ref new TextBlock();
        label->Text = Str(Num(local.tm_mday));
        label->FontSize = 11;
        label->Opacity = i == 13 ? 1 : 0.6;
        label->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Center;
        Grid::SetColumn(label, i);
        ChartLabels->Children->Append(label);
    }
    ChartNote->Text = active ? Str(L"Most " + Span(most * 60) + L" a day  ·  average " + Span(sum * 60 / active) + L" on reading days"
        + (readingDays ? L"  ·  " + Span(total / readingDays) + L" per reading day overall" : L""))
        : ref new String(L"No reading recorded in the last two weeks.");

    std::vector<Book^> books;
    for (auto b : Library::Books())
        if (Stats::Book(b->Id).seconds > 0) books.push_back(b);
    std::sort(books.begin(), books.end(), [](Book^ a, Book^ b) { return Stats::Book(a->Id).last > Stats::Book(b->Id).last; });
    auto rows = ref new Vector<StatRow^>();
    for (auto b : books)
    {
        auto s = Stats::Book(b->Id);
        auto row = ref new StatRow();
        row->Title = b->Title;
        row->Cover = b->Cover;
        row->book = b;
        row->Line1 = Str(Join({ Span(s.seconds), Num(s.pages) + L" pages", Num(s.sessions) + (s.sessions == 1 ? L" session" : L" sessions") }));
        std::wstring pct = b->Percent > 0 ? Num((int)(b->Percent + 0.5)) + L"%" : L"";
        row->Line2 = Str(s.finished > 0 ? Join({ L"Finished " + Date(s.finished), Speed(s) }) : Join({ pct, Speed(s), Left(b, s) }));
        rows->Append(row);
    }
    BookList->ItemsSource = rows;
    NoBooks->Visibility = rows->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
}

void StatsPage::OnBookClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto row = dynamic_cast<StatRow^>(e->ClickedItem);
    if (row && row->book) ShowBook(row->book);
}

void StatsPage::ShowBook(Book^ book)
{
    Stats::Load().then([book]
    {
        auto s = Stats::Book(book->Id);
        std::wstring text;
        auto line = [&text](const wchar_t* name, const std::wstring& value)
        {
            if (value.empty()) return;
            text += std::wstring(name) + L": " + value + L"\n";
        };
        if (s.seconds <= 0 && s.sessions == 0) text = L"You haven't read this book yet.";
        else
        {
            line(L"Reading time", Span(s.seconds));
            line(L"Pages turned", Num(s.pages));
            line(L"Sessions", Num(s.sessions));
            if (s.sessions > 0) line(L"Average session", Span(s.seconds / s.sessions));
            line(L"Speed", Speed(s));
            line(L"Progress", book->Percent > 0 ? Num((int)(book->Percent + 0.5)) + L"%" : L"");
            if (s.finished <= 0) line(L"Time left", Left(book, s));
            line(L"Started", Date(s.first));
            line(L"Last read", Date(s.last));
            line(L"Finished", Date(s.finished));
        }
        auto body = ref new TextBlock();
        body->Text = Str(text);
        body->TextWrapping = Windows::UI::Xaml::TextWrapping::Wrap;
        auto dialog = ref new ContentDialog();
        dialog->Title = book->Title;
        dialog->Content = body;
        dialog->PrimaryButtonText = "Close";
        dialog->ShowAsync();
    }, task_continuation_context::use_current());
}
