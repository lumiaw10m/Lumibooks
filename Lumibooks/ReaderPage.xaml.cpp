#include "pch.h"
#include "ReaderPage.xaml.h"
#include "Settings.h"
#include "Formats.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::ApplicationModel::DataTransfer;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::System;
using namespace Windows::UI;
using namespace Windows::UI::Popups;
using namespace Windows::UI::ViewManagement;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;

static const uint32_t Palette[] = { 0xffffff, 0xf4ecd8, 0xfaf3e0, 0xe6e6e6, 0xd8e8d0, 0xdbe6f2, 0xf6dede,
    0x999999, 0x5b4636, 0x444444, 0x2b2b2b, 0x1e1e1e, 0x0f1a26, 0x000000 };

static Color Rgb(uint32_t c, uint8_t a = 255)
{
    return ColorHelper::FromArgb(a, (uint8_t)(c >> 16), (uint8_t)(c >> 8), (uint8_t)c);
}

static String^ Str(const std::wstring& s)
{
    return ref new String(s.c_str(), (unsigned int)s.size());
}

static void FillTargets(ComboBox^ box);

static String^ Fmt(const wchar_t* format, ...)
{
    wchar_t s[256];
    va_list args;
    va_start(args, format);
    vswprintf_s(s, format, args);
    va_end(args);
    return ref new String(s);
}

static Windows::UI::Xaml::Visibility Shown(bool on)
{
    return on ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
}

static bool IsShown(UIElement^ e)
{
    return e->Visibility == Windows::UI::Xaml::Visibility::Visible;
}

static int TagOf(Object^ sender)
{
    auto tag = dynamic_cast<String^>(safe_cast<FrameworkElement^>(sender)->Tag);
    return tag ? _wtoi(tag->Data()) : 0;
}

ReaderPage::ReaderPage() : chapter(0), page(0), pages(1), selStart(-1), selEnd(-1), editId(-1), loading(true), resumeSpeech(false), noteShown(false), searchToken(0), lookupToken(0)
{
    InitializeComponent();
    FillTargets(TargetBox);
    auto& s = Settings::Get();
    for (int i = 0; i < Settings::FontCount; i++) FontBox->Items->Append(ref new String(Settings::Fonts[i]));
    FontBox->SelectedIndex = s.font;
    SizeSlider->Value = s.size;
    LineSlider->Value = s.lineHeight;
    LetterSlider->Value = s.letter;
    BoldSwitch->IsOn = s.bold;
    MarginSlider->Value = s.margin;
    VMarginSlider->Value = s.vmargin;
    JustifySwitch->IsOn = s.justify;
    HyphenSwitch->IsOn = s.hyphen;
    SpacingSwitch->IsOn = s.spacing;
    ParaSlider->Value = s.para;
    IndentSlider->Value = s.indent;
    NightSwitch->IsOn = s.night;
    WarmSlider->Value = s.warmth;
    DimSlider->Value = s.dim;
    for (auto c : Palette)
    {
        FgSwatches->Children->Append(Swatch(c, true));
        BgSwatches->Children->Append(Swatch(c, false));
    }
    auto voices = SpeechSynthesizer::AllVoices;
    String^ voiceId = Str(s.voice);
    if (s.voice.empty() && SpeechSynthesizer::DefaultVoice) voiceId = SpeechSynthesizer::DefaultVoice->Id;
    for (unsigned int i = 0; i < voices->Size; i++)
    {
        auto v = voices->GetAt(i);
        VoiceBox->Items->Append(v->DisplayName + " (" + v->Language + ")");
        if (String::CompareOrdinal(v->Id, voiceId) == 0) VoiceBox->SelectedIndex = (int)i;
    }
    RateSlider->Value = s.rate;
    PitchSlider->Value = s.pitch;
    ApplyTheme();
    loading = false;
}

Button^ ReaderPage::Swatch(uint32_t color, bool text)
{
    auto b = ref new Button();
    b->Width = 32;
    b->Height = 32;
    b->MinWidth = 0;
    b->MinHeight = 0;
    b->Padding = ThicknessHelper::FromUniformLength(0);
    b->BorderThickness = ThicknessHelper::FromUniformLength(1);
    b->BorderBrush = ref new SolidColorBrush(Colors::Gray);
    b->Background = ref new SolidColorBrush(Rgb(color));
    WeakReference weak(this);
    b->Click += ref new RoutedEventHandler([weak, color, text](Object^, RoutedEventArgs^)
    {
        auto p = weak.Resolve<ReaderPage>();
        if (!p) return;
        auto& s = Settings::Get();
        if (s.theme != Settings::CustomTheme)
        {
            s.customBg = s.Bg();
            s.customFg = s.Fg();
            s.theme = Settings::CustomTheme;
        }
        if (text) s.customFg = color;
        else s.customBg = color;
        p->SettingsChanged();
    });
    return b;
}

void ReaderPage::OnNavigatedTo(NavigationEventArgs^ e)
{
    book = safe_cast<Book^>(e->Parameter);
    book->Opened = (double)_time64(nullptr);
    ReaderPage^ me = this;
    shareToken = Windows::ApplicationModel::DataTransfer::DataTransferManager::GetForCurrentView()->DataRequested +=
        ref new TypedEventHandler<Windows::ApplicationModel::DataTransfer::DataTransferManager^, Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs^>(
            [me](Windows::ApplicationModel::DataTransfer::DataTransferManager^, Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs^ args)
    {
        if (me->shareText.empty()) return;
        args->Request->Data->Properties->Title = me->book->Title;
        args->Request->Data->SetText(Str(me->shareText));
    });
    ApplicationView::GetForCurrentView()->TryEnterFullScreenMode();
    ReaderPage^ self = this;
    Library::GetFile(book->FilePath).then([](StorageFile^ f)
    {
        return FileIO::ReadBufferAsync(f);
    }).then([self](task<IBuffer^> t) -> task<void>
    {
        std::shared_ptr<Epub> e;
        try { e = OpenReflow(FromBuffer(t.get()), FormatOf(self->book->FilePath->Data())); }
        catch (Exception^) {}
        if (!e)
        {
            (ref new MessageDialog("The book file is missing or damaged.", "Can't open book"))->ShowAsync();
            if (self->Frame->CanGoBack) self->Frame->GoBack();
            return task_from_result();
        }
        self->epub = e;
        self->resolver = ref new EpubResolver(e);
        return Library::ReadText("notes/" + self->book->Id + ".json").then([self](String^ json)
        {
            self->notes.Parse(json);
            int index = self->book->ChapterIndex;
            if (index < 0 || index >= (int)self->epub->spine.size()) index = 0;
            String^ target = self->book->jump.empty() ? Fmt(L"p=%.4f", self->book->ChapterProgress) : Str(self->book->jump);
            self->book->jump.clear();
            self->Open(index, target);
        });
    });
}

void ReaderPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
    Windows::ApplicationModel::DataTransfer::DataTransferManager::GetForCurrentView()->DataRequested -= shareToken;
    if (speech) speech->Stop();
    speech = nullptr;
    ApplicationView::GetForCurrentView()->ExitFullScreenMode();
    Library::Save();
}

bool ReaderPage::HandleBack()
{
    if (IsShown(Lookup)) Lookup->Visibility = Shown(false);
    else if (noteShown)
    {
        noteShown = false;
        Script("lbCloseNote", "");
    }
    else if (IsShown(Panel)) Panel->Visibility = Shown(false);
    else if (IsShown(SelBar)) HideSelBar();
    else if (IsShown(Bar)) Bar->Visibility = Shown(false);
    else return false;
    return true;
}

void ReaderPage::Open(int index, String^ fragment)
{
    if (!epub || index < 0 || index >= (int)epub->spine.size()) return;
    if (speech && speech->IsActive())
    {
        resumeSpeech = true;
        speech->Stop();
    }
    chapter = index;
    page = 0;
    pages = 1;
    HideSelBar();
    const std::string& path = epub->spine[index];
    auto uri = Web->BuildLocalStreamUri("book", "/" + Str(Wide(path.c_str(), path.size())));
    Web->NavigateToLocalStreamUri(ref new Uri(uri->AbsoluteUri + "#" + fragment), resolver);
    UpdateStatus();
}

void ReaderPage::UpdateStatus()
{
    if (!epub) return;
    Status->Text = Fmt(L"Chapter %d of %d  \u00b7  page %d of %d", chapter + 1, (int)epub->spine.size(), page + 1, pages);
}

void ReaderPage::ApplyTheme()
{
    auto& s = Settings::Get();
    auto bg = Rgb(s.Bg());
    Root->Background = ref new SolidColorBrush(bg);
    Web->DefaultBackgroundColor = bg;
    CustomTheme->Background = ref new SolidColorBrush(Rgb(s.customBg));
    CustomTheme->Foreground = ref new SolidColorBrush(Rgb(s.customFg));
    Warm->Visibility = Shown(s.night && s.warmth > 0);
    Warm->Background = ref new SolidColorBrush(Rgb(0xff8200, (uint8_t)(s.warmth * 120 / 100)));
    Dim->Visibility = Shown(s.night && s.dim > 0);
    Dim->Background = ref new SolidColorBrush(Rgb(0, (uint8_t)(s.dim * 230 / 100)));
}

task<String^> ReaderPage::Script(String^ name, String^ arg)
{
    auto args = ref new Vector<String^>();
    args->Append(arg);
    return create_task(Web->InvokeScriptAsync(name, args)).then([](task<String^> t) -> String^
    {
        try { return t.get(); }
        catch (Exception^) { return nullptr; }
    });
}

task<void> ReaderPage::ApplySettings()
{
    if (!epub) return task_from_result();
    return Script("lbApply", Settings::Get().Json()).then([](String^) {});
}

void ReaderPage::SettingsChanged()
{
    if (loading) return;
    auto& s = Settings::Get();
    s.font = FontBox->SelectedIndex < 0 ? 0 : FontBox->SelectedIndex;
    s.size = (int)SizeSlider->Value;
    s.lineHeight = (int)LineSlider->Value;
    s.letter = (int)LetterSlider->Value;
    s.bold = BoldSwitch->IsOn;
    s.margin = (int)MarginSlider->Value;
    s.vmargin = (int)VMarginSlider->Value;
    s.justify = JustifySwitch->IsOn;
    s.hyphen = HyphenSwitch->IsOn;
    s.spacing = SpacingSwitch->IsOn;
    s.para = (int)ParaSlider->Value;
    s.indent = (int)IndentSlider->Value;
    s.night = NightSwitch->IsOn;
    s.warmth = (int)WarmSlider->Value;
    s.dim = (int)DimSlider->Value;
    s.Save();
    ApplyTheme();
    ApplySettings();
}

void ReaderPage::RenderHighlights()
{
    Script("lbHighlights", notes.ChapterJson(chapter));
}

void ReaderPage::SaveNotes()
{
    Library::WriteText("notes", book->Id + ".json", notes.Json());
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
        if (epub && !epub->spine.empty()) book->Percent = std::min(100.0, (chapter + (double)(i + 1) / n) * 100.0 / epub->spine.size());
        Library::Save();
        UpdateStatus();
        UpdateBookmark();
    }
    else if (v.compare(0, 5, L"note:") == 0) noteShown = v[5] == L'1';
    else if (v.compare(0, 4, L"sel:") == 0)
    {
        JsonObject^ o = nullptr;
        if (!JsonObject::TryParse(Str(v.substr(4)), &o)) return;
        selStart = (int)o->GetNamedNumber("s", -1);
        selEnd = (int)o->GetNamedNumber("e", -1);
        selText = o->GetNamedString("t", "")->Data();
        editId = -1;
        ShowSelBar();
    }
    else if (v.compare(0, 3, L"hl:") == 0)
    {
        auto h = notes.Find(_wtoi(v.c_str() + 3));
        if (!h) return;
        editId = h->id;
        selStart = selEnd = -1;
        selText = h->text;
        ShowSelBar();
    }
    else if (v == L"sel-clear") HideSelBar();
    else if (v == L"next-chapter") Open(chapter + 1, "p=0");
    else if (v == L"prev-chapter") Open(chapter - 1, "end");
    else if (v == L"toggle-ui")
    {
        if (IsShown(SelBar)) HideSelBar();
        else Bar->Visibility = Shown(!IsShown(Bar));
    }
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
            if (speech && speech->IsActive())
            {
                resumeSpeech = true;
                speech->Stop();
            }
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
    if (!e->IsSuccess)
    {
        resumeSpeech = false;
        UpdateSpeakButton();
        return;
    }
    ReaderPage^ self = this;
    ApplySettings().then([self]
    {
        return self->Script("lbHighlights", self->notes.ChapterJson(self->chapter));
    }).then([self](String^)
    {
        if (!self->resumeSpeech) return;
        self->resumeSpeech = false;
        self->StartSpeaking();
    });
}

void ReaderPage::OnPrevChapter(Object^ sender, RoutedEventArgs^ e)
{
    Open(chapter - 1, "p=0");
}

void ReaderPage::OnNextChapter(Object^ sender, RoutedEventArgs^ e)
{
    Open(chapter + 1, "p=0");
}

void ReaderPage::OnSelectionChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnSliderChanged(Object^ sender, RangeBaseValueChangedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnToggled(Object^ sender, RoutedEventArgs^ e)
{
    SettingsChanged();
}

void ReaderPage::OnTheme(Object^ sender, RoutedEventArgs^ e)
{
    Settings::Get().theme = TagOf(sender);
    SettingsChanged();
}

std::wstring ReaderPage::ChapterName(int c)
{
    std::wstring name;
    int best = -1;
    for (auto& t : epub->toc)
    {
        int i = epub->SpineIndex(t.path);
        if (i < 0 || i > c || i < best) continue;
        if (i > best) name = t.title;
        best = i;
    }
    if (!name.empty()) return name;
    wchar_t s[32];
    swprintf_s(s, L"Chapter %d", c + 1);
    return s;
}

int ReaderPage::FindBookmark()
{
    for (size_t i = 0; i < notes.bookmarks.size(); i++)
    {
        auto& b = notes.bookmarks[i];
        if (b.chapter == chapter && (pages <= 1 || (int)(b.progress * (pages - 1) + 0.5) == page)) return (int)i;
    }
    return -1;
}

void ReaderPage::UpdateBookmark()
{
    bool on = FindBookmark() >= 0;
    Ribbon->Visibility = Shown(on);
    MarkButton->IsChecked = ref new Box<bool>(on);
}

void ReaderPage::OnBookmark(Object^ sender, RoutedEventArgs^ e)
{
    int i = FindBookmark();
    if (i >= 0)
    {
        notes.bookmarks.erase(notes.bookmarks.begin() + i);
        SaveNotes();
        UpdateBookmark();
        return;
    }
    ReaderPage^ self = this;
    int c = chapter;
    double p = pages > 1 ? (double)page / (pages - 1) : 0;
    Script("lbSnippet", "").then([self, c, p](String^ text)
    {
        Bookmark b;
        b.chapter = c;
        b.progress = p;
        b.label = self->ChapterName(c);
        if (text) b.text = text->Data();
        self->notes.bookmarks.push_back(b);
        self->SaveNotes();
        self->UpdateBookmark();
    });
}

void ReaderPage::ShowSelBar()
{
    Highlight* h = nullptr;
    if (editId >= 0) h = notes.Find(editId);
    SelText->Text = Str(selText);
    SelNote->Text = h ? Str(h->note) : ref new String();
    SelNote->Visibility = Shown(h && !h->note.empty());
    DeleteButton->Visibility = Shown(h != nullptr);
    Bar->Visibility = Shown(false);
    SelBar->Visibility = Shown(true);
}

void ReaderPage::HideSelBar()
{
    bool wasOpen = IsShown(SelBar);
    SelBar->Visibility = Shown(false);
    selStart = selEnd = editId = -1;
    selText.clear();
    if (wasOpen) Script("lbSelClear", "");
}

void ReaderPage::AddHighlight(int color, const std::wstring& note)
{
    if (selStart < 0 || selEnd <= selStart) return;
    Highlight h;
    h.id = notes.NextId();
    h.chapter = chapter;
    h.start = selStart;
    h.end = selEnd;
    h.color = color;
    h.text = selText;
    h.note = note;
    h.label = ChapterName(chapter);
    notes.highlights.push_back(h);
}

void ReaderPage::OnHighlightColor(Object^ sender, RoutedEventArgs^ e)
{
    int color = TagOf(sender);
    Highlight* h = nullptr;
    if (editId >= 0) h = notes.Find(editId);
    if (h) h->color = color;
    else AddHighlight(color, L"");
    SaveNotes();
    HideSelBar();
    RenderHighlights();
}

void ReaderPage::OnNote(Object^ sender, RoutedEventArgs^ e)
{
    Highlight* h = nullptr;
    if (editId >= 0) h = notes.Find(editId);
    auto box = ref new TextBox();
    box->AcceptsReturn = true;
    box->TextWrapping = Windows::UI::Xaml::TextWrapping::Wrap;
    box->Height = 140;
    if (h) box->Text = Str(h->note);
    auto dialog = ref new ContentDialog();
    dialog->Title = "Note";
    dialog->Content = box;
    dialog->PrimaryButtonText = "Save";
    dialog->SecondaryButtonText = "Cancel";
    ReaderPage^ self = this;
    create_task(dialog->ShowAsync()).then([self, box](ContentDialogResult r)
    {
        if (r != ContentDialogResult::Primary) return;
        std::wstring note = box->Text->Data();
        Highlight* h = nullptr;
        if (self->editId >= 0) h = self->notes.Find(self->editId);
        if (h) h->note = note;
        else self->AddHighlight(1, note);
        self->SaveNotes();
        self->HideSelBar();
        self->RenderHighlights();
    });
}

void ReaderPage::OnCopy(Object^ sender, RoutedEventArgs^ e)
{
    auto data = ref new DataPackage();
    data->SetText(Str(selText));
    Clipboard::SetContent(data);
    HideSelBar();
}

void ReaderPage::OnDeleteHighlight(Object^ sender, RoutedEventArgs^ e)
{
    auto& list = notes.highlights;
    int id = editId;
    list.erase(std::remove_if(list.begin(), list.end(), [id](const Highlight& h) { return h.id == id; }), list.end());
    SaveNotes();
    HideSelBar();
    RenderHighlights();
}

void ReaderPage::OnSelClose(Object^ sender, RoutedEventArgs^ e)
{
    HideSelBar();
}

void ReaderPage::FillPanel()
{
    auto toc = ref new Vector<NavItem^>();
    for (auto& t : epub->toc)
    {
        int i = epub->SpineIndex(t.path);
        if (i < 0) continue;
        auto item = ref new NavItem();
        item->Title = Str(t.title);
        item->Indent = ThicknessHelper::FromLengths(t.depth * 16.0, 8, 0, 8);
        item->kind = 0;
        item->chapter = i;
        item->target = t.fragment.empty() ? ref new String(L"p=0") : Uri::EscapeComponent(Str(Wide(t.fragment.c_str(), t.fragment.size())));
        toc->Append(item);
    }
    if (!toc->Size)
    {
        for (int i = 0; i < (int)epub->spine.size(); i++)
        {
            auto item = ref new NavItem();
            item->Title = Fmt(L"Section %d", i + 1);
            item->Indent = ThicknessHelper::FromLengths(0, 8, 0, 8);
            item->kind = 0;
            item->chapter = i;
            item->target = "p=0";
            toc->Append(item);
        }
    }
    TocList->ItemsSource = toc;

    notes.Sort();
    auto marks = ref new Vector<NavItem^>();
    for (size_t i = 0; i < notes.bookmarks.size(); i++)
    {
        auto& b = notes.bookmarks[i];
        auto item = ref new NavItem();
        item->Title = b.text.empty() ? ref new String(L"Bookmark") : Str(b.text);
        item->Detail = Str(ChapterName(b.chapter)) + Fmt(L"  \u00b7  %d%%", (int)(b.progress * 100));
        item->kind = 1;
        item->chapter = b.chapter;
        item->id = (int)i;
        item->target = Fmt(L"p=%.4f", b.progress);
        marks->Append(item);
    }
    MarkList->ItemsSource = marks;

    std::vector<const Highlight*> sorted;
    for (auto& h : notes.highlights) sorted.push_back(&h);
    std::sort(sorted.begin(), sorted.end(), [](const Highlight* a, const Highlight* b)
    {
        return a->chapter != b->chapter ? a->chapter < b->chapter : a->start < b->start;
    });
    auto hls = ref new Vector<NavItem^>();
    for (auto h : sorted)
    {
        auto item = ref new NavItem();
        item->Title = Str(h->text);
        item->Detail = h->note.empty() ? Str(ChapterName(h->chapter)) : Str(h->note);
        item->kind = 2;
        item->chapter = h->chapter;
        item->id = h->id;
        item->target = Fmt(L"o=%d", h->start);
        hls->Append(item);
    }
    NoteList->ItemsSource = hls;
}

void ReaderPage::OnContents(Object^ sender, RoutedEventArgs^ e)
{
    if (!epub) return;
    FillPanel();
    Bar->Visibility = Shown(false);
    Panel->Visibility = Shown(true);
}

void ReaderPage::OnNavItemClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto item = safe_cast<NavItem^>(e->ClickedItem);
    Panel->Visibility = Shown(false);
    Open(item->chapter, item->target);
}

void ReaderPage::ShowDeleteMenu(FrameworkElement^ target)
{
    if (!target) return;
    auto item = dynamic_cast<NavItem^>(target->DataContext);
    if (!item || item->kind == 0) return;
    auto remove = ref new MenuFlyoutItem();
    remove->Text = "Delete";
    WeakReference weak(this);
    remove->Click += ref new RoutedEventHandler([weak, item](Object^, RoutedEventArgs^)
    {
        auto p = weak.Resolve<ReaderPage>();
        if (!p) return;
        if (item->kind == 1 && item->id < (int)p->notes.bookmarks.size())
            p->notes.bookmarks.erase(p->notes.bookmarks.begin() + item->id);
        else if (item->kind == 2)
        {
            auto& list = p->notes.highlights;
            int id = item->id;
            list.erase(std::remove_if(list.begin(), list.end(), [id](const Highlight& h) { return h.id == id; }), list.end());
            p->RenderHighlights();
        }
        p->SaveNotes();
        p->FillPanel();
        p->UpdateBookmark();
    });
    auto menu = ref new MenuFlyout();
    menu->Items->Append(remove);
    menu->ShowAt(target);
}

void ReaderPage::OnNavItemHolding(Object^ sender, HoldingRoutedEventArgs^ e)
{
    if (e->HoldingState != Windows::UI::Input::HoldingState::Started) return;
    e->Handled = true;
    ShowDeleteMenu(dynamic_cast<FrameworkElement^>(e->OriginalSource));
}

void ReaderPage::OnNavItemRightTapped(Object^ sender, RightTappedRoutedEventArgs^ e)
{
    if (e->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Touch) return;
    e->Handled = true;
    ShowDeleteMenu(dynamic_cast<FrameworkElement^>(e->OriginalSource));
}

void ReaderPage::StartSpeaking()
{
    if (!epub) return;
    if (!speech)
    {
        WeakReference weak(this);
        try
        {
            speech = ref new ReadAloud(book, [weak](int i)
            {
                auto p = weak.Resolve<ReaderPage>();
                if (p) p->Script("lbHighlight", Str(std::to_wstring(i)));
            }, [weak]()
            {
                auto p = weak.Resolve<ReaderPage>();
                if (p) p->OnSpeechEnd();
            });
        }
        catch (Exception^)
        {
            (ref new MessageDialog("No speech voices are installed.", "Can't read aloud"))->ShowAsync();
            return;
        }
    }
    ReaderPage^ self = this;
    Script("lbSentences", "page").then([self](String^ json)
    {
        JsonObject^ o = nullptr;
        if (!self->speech || !json || !JsonObject::TryParse(json, &o)) return;
        auto arr = o->GetNamedArray("s");
        std::vector<std::wstring> list;
        for (unsigned int i = 0; i < arr->Size; i++) list.push_back(arr->GetStringAt(i)->Data());
        self->speech->Start(std::move(list), (int)o->GetNamedNumber("start", 0));
        self->UpdateSpeakButton();
    });
}

void ReaderPage::StopSpeaking()
{
    resumeSpeech = false;
    if (speech) speech->Stop();
    Script("lbHighlight", "-1");
    UpdateSpeakButton();
}

void ReaderPage::OnSpeechEnd()
{
    if (epub && chapter + 1 < (int)epub->spine.size())
    {
        resumeSpeech = true;
        Open(chapter + 1, "p=0");
    }
    else StopSpeaking();
}

void ReaderPage::UpdateSpeakButton()
{
    bool on = (speech && speech->IsActive()) || resumeSpeech;
    SpeakButton->Icon = ref new SymbolIcon(on ? Symbol::Stop : Symbol::Play);
    if (on) SpeakButton->Label = "Stop reading";
    else SpeakButton->Label = "Read aloud";
}

void ReaderPage::OnSpeak(Object^ sender, RoutedEventArgs^ e)
{
    if ((speech && speech->IsActive()) || resumeSpeech) StopSpeaking();
    else StartSpeaking();
}

void ReaderPage::VoiceSettingsChanged()
{
    if (loading) return;
    auto& s = Settings::Get();
    int i = VoiceBox->SelectedIndex;
    auto voices = SpeechSynthesizer::AllVoices;
    if (i >= 0 && i < (int)voices->Size) s.voice = voices->GetAt(i)->Id->Data();
    s.rate = (int)RateSlider->Value;
    s.pitch = (int)PitchSlider->Value;
    s.Save();
    if (speech) speech->Restart();
}

void ReaderPage::OnVoiceChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    VoiceSettingsChanged();
}

void ReaderPage::OnVoiceSliderChanged(Object^ sender, RangeBaseValueChangedEventArgs^ e)
{
    VoiceSettingsChanged();
}

static const wchar_t* const Targets[][2] = {
    { L"en", L"English" }, { L"es", L"Español" }, { L"pt", L"Português" }, { L"fr", L"Français" }, { L"de", L"Deutsch" },
    { L"it", L"Italiano" }, { L"nl", L"Nederlands" }, { L"pl", L"Polski" }, { L"ru", L"Русский" },
    { L"uk", L"Українська" }, { L"cs", L"Čeština" }, { L"sv", L"Svenska" },
    { L"tr", L"Türkçe" }, { L"el", L"Ελληνικά" }, { L"ar", L"العربية" },
    { L"zh", L"中文" }, { L"ja", L"日本語" }, { L"ko", L"한국어" }, { L"hi", L"हिन्दी" } };
static const int TargetCount = sizeof(Targets) / sizeof(Targets[0]);

static std::wstring Lang2(const std::wstring& tag)
{
    std::wstring r;
    for (auto c : tag)
    {
        if (c == L'-' || c == L'_') break;
        r += towlower(c);
    }
    return r.size() == 2 ? r : std::wstring();
}

static void FillTargets(ComboBox^ box)
{
    std::wstring want;
    auto values = ApplicationData::Current->LocalSettings->Values;
    if (values->HasKey("translateTo"))
        if (auto s = dynamic_cast<String^>(values->Lookup("translateTo"))) want = s->Data();
    if (want.empty()) want = Lang2(Windows::Globalization::ApplicationLanguages::Languages->GetAt(0)->Data());
    int pick = 0;
    for (int i = 0; i < TargetCount; i++)
    {
        box->Items->Append(ref new String(Targets[i][1]));
        if (want == Targets[i][0]) pick = i;
    }
    box->SelectedIndex = pick;
}

void ReaderPage::OnSearchOpen(Object^ sender, RoutedEventArgs^ e)
{
    if (!epub) return;
    FillPanel();
    Bar->Visibility = Shown(false);
    Panel->Visibility = Shown(true);
    PanelPivot->SelectedIndex = 1;
    SearchBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void ReaderPage::OnSearchKey(Object^ sender, KeyRoutedEventArgs^ e)
{
    if (e->Key != Windows::System::VirtualKey::Enter || !epub) return;
    e->Handled = true;
    std::wstring q = SearchBox->Text->Data();
    size_t a = q.find_first_not_of(L" \t"), z = q.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return;
    q = q.substr(a, z - a + 1);
    int token = ++searchToken;
    SearchStatus->Text = L"Searching…";
    SearchList->ItemsSource = nullptr;
    SearchList->Focus(Windows::UI::Xaml::FocusState::Programmatic);
    auto book = epub;
    ReaderPage^ self = this;
    create_task([book, q] { return book->Search(q, 300); }).then([self, token, q](std::vector<SearchHit> hits)
    {
        if (token != self->searchToken) return;
        auto items = ref new Vector<NavItem^>();
        String^ escaped = Uri::EscapeComponent(Str(q));
        for (auto& h : hits)
        {
            auto item = ref new NavItem();
            item->Title = Str(h.snippet);
            item->Detail = Str(self->ChapterName(h.chapter));
            item->kind = 3;
            item->chapter = h.chapter;
            item->target = Fmt(L"q=%d:", h.k) + escaped;
            items->Append(item);
        }
        self->SearchList->ItemsSource = items;
        if (hits.empty()) self->SearchStatus->Text = L"No results";
        else if (hits.size() >= 300) self->SearchStatus->Text = L"300+ results";
        else self->SearchStatus->Text = Fmt(hits.size() == 1 ? L"%d result" : L"%d results", (int)hits.size());
    }, task_continuation_context::use_current());
}

void ReaderPage::OnShare(Object^ sender, RoutedEventArgs^ e)
{
    shareText = selText;
    HideSelBar();
    Windows::ApplicationModel::DataTransfer::DataTransferManager::ShowShareUI();
}

void ReaderPage::ShowLookup(String^ title, bool translate)
{
    HideSelBar();
    LookupTitle->Text = title;
    LookupText->Text = "";
    LookupBusy->IsActive = true;
    TargetBox->Visibility = Shown(translate);
    Lookup->Visibility = Shown(true);
}

void ReaderPage::OnLookupClose(Object^ sender, RoutedEventArgs^ e)
{
    lookupToken++;
    Lookup->Visibility = Shown(false);
}

static Windows::Web::Http::HttpClient^ Http()
{
    auto c = ref new Windows::Web::Http::HttpClient();
    c->DefaultRequestHeaders->UserAgent->TryParseAdd("Lumibooks/1.0");
    return c;
}

static std::wstring Plain(String^ html)
{
    std::string u = Utf8(html->Data());
    std::wstring t = HtmlText(std::vector<uint8_t>(u.begin(), u.end()));
    std::wstring o;
    bool space = false;
    for (auto c : t)
    {
        if (iswspace(c)) { space = !o.empty(); continue; }
        if (space) o += L' ';
        space = false;
        o += c;
    }
    return o;
}

static bool Punct(wchar_t c)
{
    return iswspace(c) || iswpunct(c) || c == 0x201c || c == 0x201d || c == 0x2018 || c == 0x2019 || c == 0x00ab || c == 0x00bb || c == 0x2014 || c == 0x2026;
}

void ReaderPage::OnDefine(Object^ sender, RoutedEventArgs^ e)
{
    std::wstring w = selText;
    while (!w.empty() && Punct(w.front())) w.erase(0, 1);
    while (!w.empty() && Punct(w.back())) w.pop_back();
    if (w.empty()) return;
    ShowLookup(Str(w), false);
    Define(w, false);
}

void ReaderPage::Define(const std::wstring& word, bool lower)
{
    int token = ++lookupToken;
    std::wstring w = word;
    if (lower) for (auto& c : w) c = towlower(c);
    auto uri = ref new Uri("https://en.wiktionary.org/api/rest_v1/page/definition/" + Uri::EscapeComponent(Str(w)));
    std::wstring lang = Lang2(epub ? epub->language : std::wstring());
    ReaderPage^ self = this;
    create_task(Http()->GetAsync(uri)).then([](Windows::Web::Http::HttpResponseMessage^ r)
    {
        if (!r->IsSuccessStatusCode) return task_from_result<String^>(nullptr);
        return create_task(r->Content->ReadAsStringAsync());
    }).then([self, token, word, lower, lang](task<String^> t)
    {
        if (token != self->lookupToken) return;
        String^ json = nullptr;
        try { json = t.get(); }
        catch (Exception^)
        {
            self->LookupBusy->IsActive = false;
            self->LookupText->Text = L"Could not reach Wiktionary. Check the connection.";
            return;
        }
        JsonObject^ root = nullptr;
        if (!json || !JsonObject::TryParse(json, &root) || !root->Size)
        {
            std::wstring low = word;
            for (auto& c : low) c = towlower(c);
            if (!lower && low != word) { self->Define(word, true); return; }
            self->LookupBusy->IsActive = false;
            self->LookupText->Text = L"No definition found.";
            return;
        }
        std::vector<String^> keys;
        if (!lang.empty() && root->HasKey(Str(lang))) keys.push_back(Str(lang));
        if (lang != L"en" && root->HasKey("en")) keys.push_back("en");
        for (auto kv : root)
            if (String::CompareOrdinal(kv->Key, Str(lang)) != 0 && String::CompareOrdinal(kv->Key, "en") != 0) keys.push_back(kv->Key);
        std::wstring out;
        int groups = 0;
        for (auto k : keys)
        {
            if (groups >= 3) break;
            auto v = root->GetNamedValue(k);
            if (v->ValueType != JsonValueType::Array) continue;
            auto arr = v->GetArray();
            for (unsigned i = 0; i < arr->Size && i < 4; i++)
            {
                if (arr->GetAt(i)->ValueType != JsonValueType::Object) continue;
                auto entry = arr->GetObjectAt(i);
                std::wstring head = entry->GetNamedString("language", "")->Data();
                std::wstring pos = entry->GetNamedString("partOfSpeech", "")->Data();
                auto defs = entry->GetNamedArray("definitions", ref new JsonArray());
                std::wstring body;
                int n = 0;
                for (unsigned j = 0; j < defs->Size && n < 5; j++)
                {
                    if (defs->GetAt(j)->ValueType != JsonValueType::Object) continue;
                    auto d = Plain(defs->GetObjectAt(j)->GetNamedString("definition", ""));
                    if (d.empty()) continue;
                    wchar_t num[8];
                    swprintf_s(num, L"%d. ", ++n);
                    body += num + d + L"\n";
                }
                if (body.empty()) continue;
                if (!out.empty()) out += L"\n";
                out += head + (pos.empty() ? std::wstring() : L" · " + pos) + L"\n" + body;
            }
            groups++;
        }
        self->LookupBusy->IsActive = false;
        if (out.empty()) self->LookupText->Text = L"No definition found.";
        else self->LookupText->Text = Str(out + L"\nSource: Wiktionary");
    }, task_continuation_context::use_current());
}

void ReaderPage::OnTranslate(Object^ sender, RoutedEventArgs^ e)
{
    shareText = selText;
    if (shareText.empty()) return;
    ShowLookup("Translation", true);
    Translate();
}

void ReaderPage::OnTargetChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    int i = TargetBox->SelectedIndex;
    if (i < 0) return;
    ApplicationData::Current->LocalSettings->Values->Insert("translateTo", ref new String(Targets[i][0]));
    if (IsShown(Lookup) && IsShown(TargetBox)) Translate();
}

void ReaderPage::Translate()
{
    int i = TargetBox->SelectedIndex;
    if (i < 0 || shareText.empty()) return;
    int token = ++lookupToken;
    std::wstring target = Targets[i][0], source = Lang2(epub ? epub->language : std::wstring());
    LookupBusy->IsActive = true;
    LookupText->Text = "";
    if (source == target)
    {
        LookupBusy->IsActive = false;
        LookupText->Text = L"This book is already in that language. Pick another one above.";
        return;
    }
    std::wstring text = shareText.size() > 480 ? shareText.substr(0, 480) : shareText;
    String^ from = source.empty() ? ref new String(L"Autodetect") : Str(source);
    auto uri = ref new Uri("https://api.mymemory.translated.net/get?q=" + Uri::EscapeComponent(Str(text)) + "&langpair=" + from + "%7C" + Str(target));
    ReaderPage^ self = this;
    create_task(Http()->GetStringAsync(uri)).then([self, token](task<String^> t)
    {
        if (token != self->lookupToken) return;
        self->LookupBusy->IsActive = false;
        String^ json = nullptr;
        try { json = t.get(); }
        catch (Exception^)
        {
            self->LookupText->Text = L"Could not reach the translation service. Check the connection.";
            return;
        }
        JsonObject^ root = nullptr;
        String^ result = nullptr;
        if (json && JsonObject::TryParse(json, &root) && root->HasKey("responseData") && root->GetNamedValue("responseData")->ValueType == JsonValueType::Object)
            result = root->GetNamedObject("responseData")->GetNamedString("translatedText", "");
        std::wstring r = result ? result->Data() : L"";
        bool bad = r.empty() || r.find(L"INVALID") != std::wstring::npos || r.find(L"MYMEMORY WARNING") != std::wstring::npos;
        if (bad) self->LookupText->Text = L"The translation is not available right now.";
        else self->LookupText->Text = Str(Plain(result) + L"\n\nSource: MyMemory");
    }, task_continuation_context::use_current());
}
