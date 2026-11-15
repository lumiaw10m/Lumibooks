#pragma once
#include "LibraryPage.g.h"
#include "Library.h"

namespace Lumibooks
{
    public ref class LibraryPage sealed
    {
    public:
        LibraryPage();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        struct ShelfView
        {
            Platform::Collections::Vector<Book^>^ books;
            Windows::UI::Xaml::Controls::TextBlock^ empty;
        };
        std::vector<ShelfView> views;
        Windows::UI::Xaml::Controls::ContentControl^ continueCard;
        bool building;
        int sort;
        std::wstring Current();
        void Refresh();
        void RebuildShelves(const std::wstring& select);
        void AskName(Platform::String^ title, Platform::String^ initial, std::function<void(const std::wstring&)> done);
        void ShowMenu(Windows::UI::Xaml::FrameworkElement^ target);
        void OnItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnImport(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNotes(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSearch(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSearchChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
        void OnSearchKey(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnSort(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnContinue(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnNewShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRenameShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDeleteShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnShelfChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnHolding(Platform::Object^ sender, Windows::UI::Xaml::Input::HoldingRoutedEventArgs^ e);
        void OnRightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
    };
}
