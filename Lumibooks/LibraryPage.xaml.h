#pragma once
#include "LibraryPage.g.h"
#include "Library.h"

namespace Lumibooks
{
    public ref class LibraryPage sealed
    {
    public:
        LibraryPage();
        property Windows::Foundation::Collections::IObservableVector<Book^>^ View
        {
            Windows::Foundation::Collections::IObservableVector<Book^>^ get() { return view; }
        }

    private:
        Platform::Collections::Vector<Book^>^ view;
        bool building;
        std::wstring Current();
        void Refresh();
        void RebuildShelves(const std::wstring& select);
        void AskName(Platform::String^ title, Platform::String^ initial, std::function<void(const std::wstring&)> done);
        void ShowMenu(Windows::UI::Xaml::FrameworkElement^ target);
        void OnItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnImport(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnNewShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnRenameShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnDeleteShelf(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnShelfChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnHolding(Platform::Object^ sender, Windows::UI::Xaml::Input::HoldingRoutedEventArgs^ e);
        void OnRightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
    };
}
