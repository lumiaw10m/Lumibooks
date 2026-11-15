#pragma once
#include "NotesPage.g.h"
#include "FixedReaderPage.xaml.h"

namespace Lumibooks
{
    public ref class NotesPage sealed
    {
    public:
        NotesPage();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        void OnItemClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
    };
}
