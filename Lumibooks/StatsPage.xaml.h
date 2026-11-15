#pragma once
#include "StatsPage.g.h"
#include "Library.h"

namespace Lumibooks
{
    public ref class StatRow sealed
    {
    public:
        property Platform::String^ Title;
        property Platform::String^ Line1;
        property Platform::String^ Line2;
        property Windows::UI::Xaml::Media::ImageSource^ Cover;

    internal:
        Book^ book;
    };

    public ref class StatsPage sealed
    {
    public:
        StatsPage();

    internal:
        static void ShowBook(Book^ book);

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        void Fill();
        void OnBookClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
    };
}
