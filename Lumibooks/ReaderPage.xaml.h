#pragma once
#include "ReaderPage.g.h"
#include "Library.h"
#include "Annotations.h"
#include "EpubResolver.h"
#include "ReadAloud.h"

namespace Lumibooks
{
    public ref class NavItem sealed
    {
    public:
        property Platform::String^ Title;
        property Platform::String^ Detail;
        property Windows::UI::Xaml::Thickness Indent;
        property Windows::UI::Xaml::Media::Brush^ Accent;

    internal:
        int kind, chapter, id;
        double progress;
        Platform::String^ target;
        Book^ book;
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class NoteGroup sealed
    {
    public:
        NoteGroup() { Items = ref new Platform::Collections::Vector<NavItem^>(); }
        property Platform::String^ Title;
        property Windows::Foundation::Collections::IObservableVector<NavItem^>^ Items;
    };

    public ref class ReaderPage sealed
    {
    public:
        ReaderPage();
        bool HandleBack();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
        virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        Book^ book;
        std::shared_ptr<Epub> epub;
        EpubResolver^ resolver;
        ReadAloud^ speech;
        Annotations notes;
        int chapter, page, pages, selStart, selEnd, editId;
        std::wstring selText;
        bool loading, resumeSpeech, noteShown;
        int searchToken, lookupToken;
        std::wstring shareText;
        Windows::Foundation::EventRegistrationToken shareToken;

        void ShowLookup(Platform::String^ title, bool translate);
        void Define(const std::wstring& word, bool lower);
        void Translate();
        void SaveProgress();
        void OnSearchOpen(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSearchKey(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnShare(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDefine(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTranslate(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTargetChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnLookupClose(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

        void Open(int index, Platform::String^ fragment);
        void UpdateStatus();
        void ApplyTheme();
        Windows::UI::Xaml::Controls::Button^ Swatch(uint32_t color, bool text);
        concurrency::task<Platform::String^> Script(Platform::String^ name, Platform::String^ arg);
        concurrency::task<void> ApplySettings();
        void SettingsChanged();
        void VoiceSettingsChanged();
        void StartSpeaking();
        void StopSpeaking();
        void OnSpeechEnd();
        void UpdateSpeakButton();
        void SaveNotes();
        void RenderHighlights();
        int FindBookmark();
        std::wstring ChapterName(int c);
        void UpdateBookmark();
        void ShowSelBar();
        void HideSelBar();
        void FillPanel();
        void AddHighlight(int color, const std::wstring& note);
        void ShowDeleteMenu(Windows::UI::Xaml::FrameworkElement^ target);

        void OnScriptNotify(Platform::Object^ sender, Windows::UI::Xaml::Controls::NotifyEventArgs^ e);
        void OnNavigationStarting(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationStartingEventArgs^ e);
        void OnNavigationCompleted(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationCompletedEventArgs^ e);
        void OnPrevChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNextChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnContents(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnBookmark(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSelectionChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnTheme(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSpeak(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnVoiceChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnVoiceSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnHighlightColor(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNote(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnCopy(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDeleteHighlight(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSelClose(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNavItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnNavItemHolding(Platform::Object^ sender, Windows::UI::Xaml::Input::HoldingRoutedEventArgs^ e);
        void OnNavItemRightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
    };
}
