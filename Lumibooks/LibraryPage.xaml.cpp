#include "pch.h"
#include "LibraryPage.xaml.h"
#include "ReaderPage.xaml.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;

LibraryPage::LibraryPage()
{
    InitializeComponent();
    WeakReference weak(this);
    Library::Books()->VectorChanged += ref new VectorChangedEventHandler<Book^>([weak](IObservableVector<Book^>^, IVectorChangedEventArgs^)
    {
        auto page = weak.Resolve<LibraryPage>();
        if (page) page->UpdateEmpty();
    });
    UpdateEmpty();
}

void LibraryPage::UpdateEmpty()
{
    EmptyText->Visibility = Library::Books()->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
}

void LibraryPage::OnItemClick(Object^ sender, ItemClickEventArgs^ e)
{
    this->Frame->Navigate(TypeName(ReaderPage::typeid), e->ClickedItem);
}

void LibraryPage::OnImport(Object^ sender, RoutedEventArgs^ e)
{
    auto picker = ref new FileOpenPicker();
    picker->SuggestedStartLocation = PickerLocationId::DocumentsLibrary;
    picker->ViewMode = PickerViewMode::List;
    picker->FileTypeFilter->Append(".epub");
    create_task(picker->PickMultipleFilesAsync()).then([](IVectorView<StorageFile^>^ files)
    {
        auto chain = task_from_result();
        auto failed = std::make_shared<std::wstring>();
        auto ui = task_continuation_context::use_current();
        for (auto file : files)
        {
            chain = chain.then([file, failed]
            {
                return Library::Import(file).then([file, failed](task<Book^> t)
                {
                    try { t.get(); }
                    catch (Exception^) { *failed += std::wstring(file->Name->Data()) + L"\n"; }
                });
            }, ui);
        }
        return chain.then([failed]
        {
            if (!failed->empty())
                (ref new MessageDialog(ref new String(failed->c_str()), "Couldn't import"))->ShowAsync();
        }, ui);
    });
}

void LibraryPage::ShowRemove(FrameworkElement^ target)
{
    auto book = dynamic_cast<Book^>(target->DataContext);
    if (!book) return;
    auto item = ref new MenuFlyoutItem();
    item->Text = "Remove";
    item->Click += ref new RoutedEventHandler([book](Object^, RoutedEventArgs^) { Library::Remove(book); });
    auto menu = ref new MenuFlyout();
    menu->Items->Append(item);
    menu->ShowAt(target);
}

void LibraryPage::OnHolding(Object^ sender, HoldingRoutedEventArgs^ e)
{
    if (e->HoldingState != Windows::UI::Input::HoldingState::Started) return;
    e->Handled = true;
    ShowRemove(safe_cast<FrameworkElement^>(sender));
}

void LibraryPage::OnRightTapped(Object^ sender, RightTappedRoutedEventArgs^ e)
{
    if (e->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Touch) return;
    e->Handled = true;
    ShowRemove(safe_cast<FrameworkElement^>(sender));
}
