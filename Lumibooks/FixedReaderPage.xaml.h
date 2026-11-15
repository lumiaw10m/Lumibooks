#pragma once
#include "FixedReaderPage.g.h"
#include "Library.h"
#include "Formats.h"
#include "ReaderPage.xaml.h"

namespace Lumibooks
{
    public ref class PageView sealed : Windows::UI::Xaml::Data::INotifyPropertyChanged
    {
    public:
        virtual event Windows::UI::Xaml::Data::PropertyChangedEventHandler^ PropertyChanged;
        property Windows::UI::Xaml::Media::ImageSource^ Image
        {
            Windows::UI::Xaml::Media::ImageSource^ get() { return image; }
            void set(Windows::UI::Xaml::Media::ImageSource^ v)
            {
                image = v;
                PropertyChanged(this, ref new Windows::UI::Xaml::Data::PropertyChangedEventArgs("Image"));
            }
        }

    internal:
        int index = 0;
        float scale = 0;
        int pending = 0;
        std::string mark;

    private:
        Windows::UI::Xaml::Media::ImageSource^ image;
    };

    public ref class FixedReaderPage sealed
    {
    public:
        FixedReaderPage();
        bool HandleBack();

    protected:
        virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
        virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

    private:
        Book^ book;
        std::shared_ptr<FixedDoc> doc;
        Platform::Collections::Vector<PageView^>^ views;
        float fitW, fitH;
        bool ready;
        Annotations notes;
        std::string mark;
        int searchToken = 0;
        int statPage = -1;
        std::shared_ptr<bool> searchCancel;

        void Show(int page);
        std::wstring SectionOf(int page);
        int FindBookmark();
        void UpdateBookmark();
        void FillPanel();
        void SaveNotes();
        void ShowDeleteMenu(Windows::UI::Xaml::FrameworkElement^ target);
        void OnContents(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSearchOpen(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnSearchKey(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
        void OnBookmark(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
        void OnEntryClick(Platform::Object^ sender, Windows::UI::Xaml::Controls::ItemClickEventArgs^ e);
        void OnMarkHolding(Platform::Object^ sender, Windows::UI::Xaml::Input::HoldingRoutedEventArgs^ e);
        void OnMarkRightTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::RightTappedRoutedEventArgs^ e);
        void Render(PageView^ v, float zoom);
        float FitScale(int page);
        void UpdateStatus();
        void OnPageChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::SelectionChangedEventArgs^ e);
        void OnViewerSizeChanged(Platform::Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e);
        void OnViewChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::ScrollViewerViewChangedEventArgs^ e);
        void OnTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs^ e);
        void OnDoubleTapped(Platform::Object^ sender, Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs^ e);
        void OnSliderDone(Platform::Object^ sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs^ e);
        void OnSliderKey(Platform::Object^ sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs^ e);
    };

    inline Windows::UI::Xaml::Interop::TypeName ReaderFor(Book^ b)
    {
        return Windows::UI::Xaml::Interop::TypeName(IsFixed(FormatOf(b->FilePath->Data())) ? FixedReaderPage::typeid : ReaderPage::typeid);
    }
}
