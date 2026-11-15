#include "pch.h"
#include "FixedReaderPage.xaml.h"
#include <robuffer.h>
#include <cmath>

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Popups;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::UI::Xaml::Navigation;

namespace
{
    struct Rendered
    {
        std::vector<uint8_t> px;
        int w = 0, h = 0;
        bool ok = false;
    };

    String^ Num(int n)
    {
        wchar_t s[16];
        swprintf_s(s, L"%d", n);
        return ref new String(s);
    }
}

FixedReaderPage::FixedReaderPage() : fitW(0), fitH(0), ready(false)
{
    InitializeComponent();
    views = ref new Vector<PageView^>();
}

void FixedReaderPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    book = safe_cast<Book^>(e->Parameter);
    ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();
    FixedReaderPage^ self = this;
    Library::GetFile(book->FilePath).then([](StorageFile^ f)
    {
        return FileIO::ReadBufferAsync(f);
    }).then([self](task<IBuffer^> t)
    {
        std::shared_ptr<FixedDoc> d;
        try { d = FixedDoc::Open(FromBuffer(t.get()), FormatOf(self->book->FilePath->Data())); }
        catch (Exception^) {}
        if (!d)
        {
            (ref new MessageDialog("The document file is missing or damaged.", "Can't open document"))->ShowAsync();
            if (self->Frame->CanGoBack) self->Frame->GoBack();
            return;
        }
        self->doc = d;
        for (int i = 0; i < d->Pages(); i++)
        {
            auto v = ref new PageView();
            v->index = i;
            self->views->Append(v);
        }
        Library::ReadText("notes/" + self->book->Id + ".json").then([self, d](String^ json)
        {
            self->notes.Parse(json);
            self->PageSlider->Maximum = d->Pages();
            self->Pages->ItemsSource = self->views;
            int start = self->book->ChapterIndex;
            if (start < 0 || start >= d->Pages()) start = 0;
            self->book->jump.clear();
            self->ready = true;
            if (self->Pages->SelectedIndex == start) self->Show(start);
            else self->Pages->SelectedIndex = start;
            self->UpdateStatus();
            self->UpdateBookmark();
        }, task_continuation_context::use_current());
    });
}

bool FixedReaderPage::HandleBack()
{
    if (Panel->Visibility == Windows::UI::Xaml::Visibility::Visible) Panel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    else if (Bar->Visibility == Windows::UI::Xaml::Visibility::Visible) Bar->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    else return false;
    return true;
}

std::wstring FixedReaderPage::SectionOf(int page)
{
    std::wstring name;
    int best = -1;
    for (auto& entry : doc->toc)
        if (entry.page <= page && entry.page >= best)
        {
            name = entry.title;
            best = entry.page;
        }
    return name;
}

int FixedReaderPage::FindBookmark()
{
    int page = Pages->SelectedIndex;
    for (size_t i = 0; i < notes.bookmarks.size(); i++)
        if (notes.bookmarks[i].chapter == page) return (int)i;
    return -1;
}

void FixedReaderPage::UpdateBookmark()
{
    bool on = FindBookmark() >= 0;
    Ribbon->Visibility = on ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    MarkButton->IsChecked = ref new Box<bool>(on);
}

void FixedReaderPage::SaveNotes()
{
    Library::WriteText("notes", book->Id + ".json", notes.Json());
}

void FixedReaderPage::OnBookmark(Object^ sender, RoutedEventArgs^ e)
{
    int page = Pages->SelectedIndex;
    if (!doc || page < 0) return;
    int i = FindBookmark();
    if (i >= 0) notes.bookmarks.erase(notes.bookmarks.begin() + i);
    else
    {
        Bookmark b;
        b.chapter = page;
        b.progress = doc->Pages() > 1 ? (double)page / (doc->Pages() - 1) : 0;
        b.text = SectionOf(page);
        b.label = std::wstring(L"Page ") + Num(page + 1)->Data();
        notes.bookmarks.push_back(b);
    }
    SaveNotes();
    UpdateBookmark();
}

void FixedReaderPage::FillPanel()
{
    auto toc = ref new Vector<NavItem^>();
    for (auto& entry : doc->toc)
    {
        auto item = ref new NavItem();
        item->Title = ref new String(entry.title.c_str());
        item->Detail = Num(entry.page + 1);
        item->Indent = Thickness(entry.depth * 16.0, 8, 0, 8);
        item->kind = 0;
        item->chapter = entry.page;
        toc->Append(item);
    }
    TocList->ItemsSource = toc;
    NoToc->Visibility = toc->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;

    notes.Sort();
    auto marks = ref new Vector<NavItem^>();
    for (size_t i = 0; i < notes.bookmarks.size(); i++)
    {
        auto& b = notes.bookmarks[i];
        auto item = ref new NavItem();
        item->Title = "Page " + Num(b.chapter + 1);
        item->Detail = ref new String(b.text.c_str());
        item->Indent = Thickness(0, 8, 0, 8);
        item->kind = 1;
        item->chapter = b.chapter;
        item->id = (int)i;
        marks->Append(item);
    }
    MarkList->ItemsSource = marks;
    NoMarks->Visibility = marks->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
}

void FixedReaderPage::OnContents(Object^ sender, RoutedEventArgs^ e)
{
    if (!doc) return;
    FillPanel();
    Bar->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    Panel->Visibility = Windows::UI::Xaml::Visibility::Visible;
}

void FixedReaderPage::OnEntryClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto item = dynamic_cast<NavItem^>(e->ClickedItem);
    Panel->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    if (item && item->chapter >= 0 && item->chapter < (int)views->Size) Pages->SelectedIndex = item->chapter;
}

void FixedReaderPage::ShowDeleteMenu(FrameworkElement^ target)
{
    auto item = target ? dynamic_cast<NavItem^>(target->DataContext) : nullptr;
    if (!item) return;
    auto remove = ref new MenuFlyoutItem();
    remove->Text = "Delete";
    WeakReference weak(this);
    int id = item->id;
    remove->Click += ref new RoutedEventHandler([weak, id](Object^, RoutedEventArgs^)
    {
        auto p = weak.Resolve<FixedReaderPage>();
        if (!p || id >= (int)p->notes.bookmarks.size()) return;
        p->notes.bookmarks.erase(p->notes.bookmarks.begin() + id);
        p->SaveNotes();
        p->FillPanel();
        p->UpdateBookmark();
    });
    auto menu = ref new MenuFlyout();
    menu->Items->Append(remove);
    menu->ShowAt(target);
}

void FixedReaderPage::OnMarkHolding(Object^ sender, HoldingRoutedEventArgs^ e)
{
    if (e->HoldingState != Windows::UI::Input::HoldingState::Started) return;
    e->Handled = true;
    ShowDeleteMenu(dynamic_cast<FrameworkElement^>(e->OriginalSource));
}

void FixedReaderPage::OnMarkRightTapped(Object^ sender, RightTappedRoutedEventArgs^ e)
{
    if (e->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Touch) return;
    e->Handled = true;
    ShowDeleteMenu(dynamic_cast<FrameworkElement^>(e->OriginalSource));
}

void FixedReaderPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
    ApplicationView::GetForCurrentView()->ExitFullScreenMode();
    Library::Save();
    ready = false;
    for (auto v : views)
    {
        v->pending++;
        v->Image = nullptr;
    }
}

float FixedReaderPage::FitScale(int page)
{
    float w, h;
    if (!doc || !doc->Size(page, w, h) || w <= 0 || h <= 0) return 1;
    float vw = fitW, vh = fitH;
    if (vw <= 0 || vh <= 0)
    {
        auto b = Window::Current->Bounds;
        float raw = (float)DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;
        vw = (float)b.Width * raw;
        vh = (float)b.Height * raw;
    }
    return std::min(vw / w, vh / h);
}

void FixedReaderPage::Render(PageView^ v, float zoom)
{
    if (!doc) return;
    float w, h, scale = FitScale(v->index) * zoom;
    if (doc->Size(v->index, w, h))
    {
        float big = std::max(w, h) * scale;
        if (big > 4096) scale *= 4096 / big;
    }
    if (v->Image && std::abs(v->scale - scale) < 0.01f) return;
    int token = ++v->pending;
    auto d = doc;
    int index = v->index;
    create_task([d, index, scale]
    {
        auto r = std::make_shared<Rendered>();
        r->ok = d->Render(index, scale, r->px, r->w, r->h);
        return r;
    }).then([v, token, scale](std::shared_ptr<Rendered> r)
    {
        if (token != v->pending || !r->ok || r->w <= 0 || r->h <= 0) return;
        auto bmp = ref new WriteableBitmap(r->w, r->h);
        IUnknown* unk = reinterpret_cast<IUnknown*>(bmp->PixelBuffer);
        IBufferByteAccess* access = nullptr;
        if (SUCCEEDED(unk->QueryInterface(IID_PPV_ARGS(&access))))
        {
            byte* p = nullptr;
            access->Buffer(&p);
            memcpy(p, r->px.data(), r->px.size());
            access->Release();
        }
        bmp->Invalidate();
        v->scale = scale;
        v->Image = bmp;
    }, task_continuation_context::use_current());
}

void FixedReaderPage::Show(int page)
{
    for (auto v : views)
    {
        if (std::abs(v->index - page) <= 1)
        {
            if (!v->Image || v->scale <= 0) Render(v, 1);
        }
        else if (v->Image || v->pending)
        {
            v->pending++;
            v->scale = 0;
            v->Image = nullptr;
        }
    }
}

void FixedReaderPage::UpdateStatus()
{
    int i = Pages->SelectedIndex;
    if (!doc || i < 0) return;
    Status->Text = Num(i + 1) + " / " + Num(doc->Pages());
    PageSlider->Value = i + 1;
}

void FixedReaderPage::OnPageChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    int i = Pages->SelectedIndex;
    if (!ready || i < 0) return;
    Show(i);
    book->ChapterIndex = i;
    book->ChapterProgress = doc->Pages() > 1 ? (double)i / (doc->Pages() - 1) : 0;
    Library::Save();
    UpdateStatus();
    UpdateBookmark();
}

void FixedReaderPage::OnViewerSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    float raw = (float)DisplayInformation::GetForCurrentView()->RawPixelsPerViewPixel;
    float w = (float)e->NewSize.Width * raw, h = (float)e->NewSize.Height * raw;
    if (std::abs(w - fitW) < 1 && std::abs(h - fitH) < 1) return;
    fitW = w;
    fitH = h;
    if (!ready) return;
    for (auto v : views) if (v->Image) v->scale = 0;
    Show(Pages->SelectedIndex);
}

void FixedReaderPage::OnViewChanged(Object^ sender, ScrollViewerViewChangedEventArgs^ e)
{
    if (e->IsIntermediate) return;
    auto sv = safe_cast<ScrollViewer^>(sender);
    auto v = dynamic_cast<PageView^>(sv->DataContext);
    if (!v) return;
    float zoom = sv->ZoomFactor;
    Render(v, zoom > 1.2f ? std::min(zoom, 3.0f) : 1.0f);
}

void FixedReaderPage::OnTapped(Object^ sender, TappedRoutedEventArgs^ e)
{
    if (Bar->Visibility == Windows::UI::Xaml::Visibility::Visible)
    {
        Bar->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
        return;
    }
    double x = e->GetPosition(Pages).X, w = Pages->ActualWidth;
    int i = Pages->SelectedIndex;
    if (x < w * 0.3) { if (i > 0) Pages->SelectedIndex = i - 1; }
    else if (x > w * 0.7) { if (i + 1 < (int)views->Size) Pages->SelectedIndex = i + 1; }
    else Bar->Visibility = Windows::UI::Xaml::Visibility::Visible;
}

void FixedReaderPage::OnDoubleTapped(Object^ sender, DoubleTappedRoutedEventArgs^ e)
{
    auto sv = safe_cast<ScrollViewer^>(sender);
    Bar->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
    if (sv->ZoomFactor > 1.05f)
    {
        sv->ChangeView(nullptr, nullptr, ref new Box<float>(1.0f));
        return;
    }
    auto p = e->GetPosition(sv);
    float z = 2.5f;
    sv->ChangeView(ref new Box<double>(p.X * z - sv->ViewportWidth / 2), ref new Box<double>(p.Y * z - sv->ViewportHeight / 2), ref new Box<float>(z));
}

void FixedReaderPage::OnSliderDone(Object^ sender, PointerRoutedEventArgs^ e)
{
    int i = (int)PageSlider->Value - 1;
    if (i >= 0 && i < (int)views->Size && i != Pages->SelectedIndex) Pages->SelectedIndex = i;
}

void FixedReaderPage::OnSliderKey(Object^ sender, KeyRoutedEventArgs^ e)
{
    OnSliderDone(sender, nullptr);
}
