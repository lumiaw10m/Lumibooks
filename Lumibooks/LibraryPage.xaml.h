#pragma once
#include "LibraryPage.g.h"
#include "Library.h"

namespace Lumibooks
{
    public ref class LibraryPage sealed
    {
    public:
        LibraryPage();
        property Windows::Foundation::Collections::IObservableVector<Book^>^ Books
        {
            Windows::Foundation::Collections::IObservableVector<Book^>^ get() { return Library::Books(); }
        }

    private:
        void UpdateEmpty();
        void ShowRemove(Windows::UI::Xaml::FrameworkElement^ target);
        void OnItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnImport(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnHolding(Platform::Object^ sender, Windows::UI::Xaml::Input::HoldingRoutedEventArgs^ e);
        void OnRightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
    };
}
