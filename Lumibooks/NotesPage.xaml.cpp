#include "pch.h"
#include "NotesPage.xaml.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::UI;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

static String^ Fmt(const wchar_t* format, ...)
{
    wchar_t s[64];
    va_list a;
    va_start(a, format);
    vswprintf_s(s, format, a);
    va_end(a);
    return ref new String(s);
}

static String^ Str(const std::wstring& s)
{
    return ref new String(s.c_str(), (unsigned int)s.size());
}

static Brush^ HighlightBrush(int color)
{
    static const Color colors[] = { { 255, 255, 214, 0 }, { 255, 110, 210, 110 }, { 255, 90, 170, 255 }, { 255, 255, 120, 170 } };
    return ref new SolidColorBrush(colors[color >= 1 && color <= 4 ? color - 1 : 0]);
}

NotesPage::NotesPage()
{
    InitializeComponent();
}

void NotesPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    auto marks = ref new Vector<NoteGroup^>();
    auto hls = ref new Vector<NoteGroup^>();
    auto accent = safe_cast<Brush^>(Application::Current->Resources->Lookup("SystemControlBackgroundAccentBrush"));
    auto ui = task_continuation_context::use_current();
    auto chain = task_from_result();
    for (auto b : Library::Books())
    {
        Book^ book = b;
        chain = chain.then([book]
        {
            return Library::ReadText("notes/" + book->Id + ".json");
        }, ui).then([book, marks, hls, accent](String^ json)
        {
            Annotations a;
            a.Parse(json);
            a.Sort();
            bool fixed = IsFixed(FormatOf(book->FilePath->Data()));
            if (!a.bookmarks.empty())
            {
                auto g = ref new NoteGroup();
                g->Title = book->Title;
                for (auto& m : a.bookmarks)
                {
                    auto item = ref new NavItem();
                    String^ where = m.label.empty() ? Fmt(fixed ? L"Page %d" : L"Chapter %d", m.chapter + 1) : Str(m.label);
                    if (fixed)
                    {
                        item->Title = Fmt(L"Page %d", m.chapter + 1);
                        item->Detail = Str(m.text);
                    }
                    else
                    {
                        item->Title = m.text.empty() ? ref new String(L"Bookmark") : Str(m.text);
                        item->Detail = where + Fmt(L"  ·  %d%%", (int)(m.progress * 100));
                    }
                    item->Accent = accent;
                    item->kind = 1;
                    item->chapter = m.chapter;
                    item->progress = m.progress;
                    item->book = book;
                    g->Items->Append(item);
                }
                marks->Append(g);
            }
            if (!a.highlights.empty())
            {
                auto g = ref new NoteGroup();
                g->Title = book->Title;
                for (auto& h : a.highlights)
                {
                    auto item = ref new NavItem();
                    String^ where = h.label.empty() ? Fmt(L"Chapter %d", h.chapter + 1) : Str(h.label);
                    item->Title = Str(h.text);
                    item->Detail = h.note.empty() ? where : Str(h.note) + L"  ·  " + where;
                    item->Accent = HighlightBrush(h.color);
                    item->kind = 2;
                    item->chapter = h.chapter;
                    item->id = h.start;
                    item->book = book;
                    g->Items->Append(item);
                }
                hls->Append(g);
            }
        }, ui);
    }
    NotesPage^ self = this;
    chain.then([self, marks, hls](task<void> t)
    {
        try { t.get(); }
        catch (Exception^) {}
        self->MarksSource->Source = marks;
        self->HighlightsSource->Source = hls;
        self->NoMarks->Visibility = marks->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
        self->NoHighlights->Visibility = hls->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    }, ui);
}

void NotesPage::OnItemClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto item = dynamic_cast<NavItem^>(e->ClickedItem);
    if (!item || !item->book) return;
    auto book = item->book;
    book->ChapterIndex = item->chapter;
    if (item->kind == 1)
    {
        book->ChapterProgress = item->progress;
        book->jump.clear();
    }
    else
    {
        wchar_t s[32];
        swprintf_s(s, L"o=%d", item->id);
        book->jump = s;
    }
    this->Frame->Navigate(ReaderFor(book), book);
}
