#pragma once
#include "ReaderPage.g.h"
#include "Library.h"
#include "EpubResolver.h"
#include "ReadAloud.h"

namespace Lumibooks
{
    public ref class ReaderPage sealed
    {
    public:
        ReaderPage();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
        virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        Book^ book;
        std::shared_ptr<Epub> epub;
        EpubResolver^ resolver;
        ReadAloud^ speech;
        int chapter, page, pages;
        bool loading, resumeSpeech;

        void Open(int index, Platform::String^ fragment);
        void UpdateStatus();
        void ApplyTheme();
        concurrency::task<Platform::String^> Script(Platform::String^ name, Platform::String^ arg);
        concurrency::task<void> ApplySettings();
        void SettingsChanged();
        void VoiceSettingsChanged();
        void StartSpeaking();
        void StopSpeaking();
        void OnSpeechEnd();
        void UpdateSpeakButton();
        void OnScriptNotify(Platform::Object^ sender, Windows::UI::Xaml::Controls::NotifyEventArgs^ e);
        void OnNavigationStarting(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationStartingEventArgs^ e);
        void OnNavigationCompleted(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationCompletedEventArgs^ e);
        void OnPrevChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNextChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFontChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnTheme(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnJustifyToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSpeak(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnVoiceChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnVoiceSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
    };
}
