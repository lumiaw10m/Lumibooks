#pragma once
#include "ReaderPage.g.h"
#include "Library.h"
#include "EpubResolver.h"

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
        int chapter, page, pages;
        bool loading;

        void Open(int index, Platform::String^ fragment);
        void UpdateStatus();
        void ApplyTheme();
        void ApplySettings();
        void SettingsChanged();
        void OnScriptNotify(Platform::Object^ sender, Windows::UI::Xaml::Controls::NotifyEventArgs^ e);
        void OnNavigationStarting(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationStartingEventArgs^ e);
        void OnNavigationCompleted(Windows::UI::Xaml::Controls::WebView^ sender, Windows::UI::Xaml::Controls::WebViewNavigationCompletedEventArgs^ e);
        void OnPrevChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNextChapter(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnFontChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnSliderChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
        void OnTheme(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnJustifyToggled(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
    };
}
