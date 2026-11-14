#include "pch.h"
#include "ReaderPage.xaml.h"
#include "Settings.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::System;
using namespace Windows::UI::Popups;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

ReaderPage::ReaderPage() : chapter(0), page(0), pages(1), loading(true)
{
    InitializeComponent();
    auto& s = Settings::Get();
    for (int i = 0; i < Settings::FontCount; i++) FontBox->Items->Append(ref new String(Settings::Fonts[i]));
    FontBox->SelectedIndex = s.font;
    SizeSlider->Value = s.size;
    LineSlider->Value = s.lineHeight;
    MarginSlider->Value = s.margin;
    JustifySwitch->IsOn = s.justify;
    ApplyTheme();
    loading = false;
}

void ReaderPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    book = safe_cast<Book^>(e->Parameter);
    ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();
    ReaderPage^ self = this;
    Library::GetFile(book->FilePath).then([](StorageFile^ f)
    {
        return FileIO::ReadBufferAsync(f);
    }).then([self](task<IBuffer^> t)
    {
        std::shared_ptr<Epub> e;
        try { e = Epub::Open(FromBuffer(t.get())); }
        catch (Exception^) {}
        if (!e)
        {
            (ref new MessageDialog("The book file is missing or damaged.", "Can't open book"))->ShowAsync();
            if (self->Frame->CanGoBack) self->Frame->GoBack();
            return;
        }
        self->epub = e;
        self->resolver = ref new EpubResolver(e);
        int index = self->book->ChapterIndex;
        if (index < 0 || index >= (int)e->spine.size()) index = 0;
        wchar_t frag[32];
        swprintf_s(frag, L"p=%.4f", self->book->ChapterProgress);
        self->Open(index, ref new String(frag));
    });
}

void ReaderPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
    ApplicationView::GetForCurrentView()->ExitFullScreenMode();
    Library::Save();
}

void ReaderPage::Open(int index, String^ fragment)
{
    if (!epub || index < 0 || index >= (int)epub->spine.size()) return;
    chapter = index;
    page = 0;
    pages = 1;
    const std::string& path = epub->spine[index];
    auto uri = Web->BuildLocalStreamUri("book", "/" + ref new String(Wide(path.c_str(), path.size()).c_str()));
    Web->NavigateToLocalStreamUri(ref new Uri(uri->AbsoluteUri + "#" + fragment), resolver);
    UpdateStatus();
}

void ReaderPage::UpdateStatus()
{
    if (!epub) return;
    wchar_t s[96];
    swprintf_s(s, L"Chapter %d of %d  ·  page %d of %d", chapter + 1, (int)epub->spine.size(), page + 1, pages);
    Status->Text = ref new String(s);
}

void ReaderPage::ApplyTheme()
{
    auto c = Settings::Themes[Settings::Get().theme].color;
    Root->Background = ref new SolidColorBrush(c);
    Web->DefaultBackgroundColor = c;
}

void ReaderPage::ApplySettings()
{
    if (!epub) return;
    auto args = ref new Vector<String^>();
    args->Append(Settings::Get().Json());
    create_task(Web->InvokeScriptAsync("lbApply", args)).then([](task<String^> t)
    {
        try { t.get(); }
        catch (Exception^) {}
    });
}

void ReaderPage::SettingsChanged()
{
    if (loading) return;
    auto& s = Settings::Get();
    s.font = FontBox->SelectedIndex < 0 ? 0 : FontBox->SelectedIndex;
    s.size = (int)SizeSlider->Value;
    s.lineHeight = (int)LineSlider->Value;
    s.margin = (int)MarginSlider->Value;
    s.justify = JustifySwitch->IsOn;
    s.Save();
    ApplyTheme();
    ApplySettings();
}

void ReaderPage::OnScriptNotify(Object^ sender, NotifyEventArgs^ e)
{
    std::wstring v = e->Value->Data();
    if (v.compare(0, 5, L"page:") == 0)
    {
        int i = 0, n = 1;
        if (swscanf_s(v.c_str() + 5, L"%d/%d", &i, &n) != 2 || n < 1) return;
        page = i;
        pages = n;
        book->ChapterIndex = chapter;
        book->ChapterProgress = n > 1 ? (double)i / (n - 1) : 0;
        Library::Save();
        UpdateStatus();
    }
    else if (v == L"next-chapter") Open(chapter + 1, "p=0");
    else if (v == L"prev-chapter") Open(chapter - 1, "end");
    else if (v == L"toggle-ui")
        Bar->Visibility = Bar->Visibility == Windows::UI::Xaml::Visibility::Visible ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
}

void ReaderPage::OnNavigationStarting(WebView^ sender, WebViewNavigationStartingEventArgs^ e)
{
    auto uri = e->Uri;
    if (!uri || !epub) return;
    if (String::CompareOrdinal(uri->SchemeName, "ms-local-stream") == 0)
    {
        int i = epub->SpineIndex(EpubResolver::PathOf(uri));
        if (i >= 0 && i != chapter)
        {
            chapter = i;
            UpdateStatus();
        }
        return;
    }
    e->Cancel = true;
    Launcher::LaunchUriAsync(uri);
}

void ReaderPage::OnNavigationCompleted(WebView^ sender, WebViewNavigationCompletedEventArgs^ e)
{
    if (e->IsSuccess) ApplySettings();
}

void ReaderPage::OnPrevChapter(Object^ sender, RoutedEventArgs^ e)
{
    Open(chapter - 1, "p=0");
}

void ReaderPage::OnNextChapter(Object^ sender, RoutedEventArgs^ e)
{
    Open(chapter + 1, "p=0");
}

void ReaderPage::OnFontChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnSliderChanged(Object^ sender, RangeBaseValueChangedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnJustifyToggled(Object^ sender, RoutedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnTheme(Object^ sender, RoutedEventArgs^ e)
{
    auto tag = safe_cast<String^>(safe_cast<FrameworkElement^>(sender)->Tag);
    Settings::Get().theme = _wtoi(tag->Data());
    SettingsChanged();
}
