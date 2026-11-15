#include "pch.h"
#include "LibraryPage.xaml.h"
#include "ReaderPage.xaml.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Storage;
using namespace Windows::Storage::Pickers;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Interop;

LibraryPage::LibraryPage() : building(false)
{
    view = ref new Vector<Book^>();
    InitializeComponent();
    WeakReference weak(this);
    Library::Books()->VectorChanged += ref new VectorChangedEventHandler<Book^>([weak](IObservableVector<Book^>^, IVectorChangedEventArgs^)
    {
        auto page = weak.Resolve<LibraryPage>();
        if (page) page->Refresh();
    });
    Library::Load().then([weak]
    {
        auto page = weak.Resolve<LibraryPage>();
        if (page) page->RebuildShelves(L"");
    }, task_continuation_context::use_current());
}

std::wstring LibraryPage::Current()
{
    int i = Shelves->SelectedIndex;
    auto& all = Library::Shelves();
    return i > 0 && i <= (int)all.size() ? all[i - 1] : std::wstring();
}

void LibraryPage::Refresh()
{
    if (building) return;
    auto shelf = Current();
    view->Clear();
    for (int fav = 1; fav >= 0; fav--)
        for (auto b : Library::Books())
            if (b->Favorite == (fav == 1) && (shelf.empty() || b->InShelf(shelf))) view->Append(b);
    EmptyText->Text = !Library::Books()->Size ? "Your library is empty. Use Import to add EPUB books."
        : "This shelf is empty. Hold a book and choose Shelves to add it here.";
    EmptyText->Visibility = view->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    auto shelfButtons = shelf.empty() ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    RenameShelfButton->Visibility = shelfButtons;
    DeleteShelfButton->Visibility = shelfButtons;
}

void LibraryPage::RebuildShelves(const std::wstring& select)
{
    auto& all = Library::Shelves();
    building = true;
    Shelves->Items->Clear();
    auto head = ref new PivotItem();
    head->Header = "All";
    Shelves->Items->Append(head);
    int index = 0;
    for (size_t i = 0; i < all.size(); i++)
    {
        auto item = ref new PivotItem();
        item->Header = ref new String(all[i].c_str());
        Shelves->Items->Append(item);
        if (all[i] == select) index = (int)i + 1;
    }
    Shelves->SelectedIndex = index;
    Shelves->Visibility = all.empty() ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    building = false;
    Refresh();
}

void LibraryPage::OnShelfChanged(Object^ sender, SelectionChangedEventArgs^ e)
{
    Refresh();
}

void LibraryPage::AskName(String^ title, String^ initial, std::function<void(const std::wstring&)> done)
{
    auto box = ref new TextBox();
    box->Text = initial;
    box->SelectAll();
    auto dialog = ref new ContentDialog();
    dialog->Title = title;
    dialog->Content = box;
    dialog->PrimaryButtonText = "OK";
    dialog->SecondaryButtonText = "Cancel";
    create_task(dialog->ShowAsync()).then([box, done](ContentDialogResult r)
    {
        if (r != ContentDialogResult::Primary) return;
        std::wstring name = box->Text->Data();
        size_t a = name.find_first_not_of(L" \t"), z = name.find_last_not_of(L" \t");
        if (a == std::wstring::npos) return;
        done(name.substr(a, z - a + 1));
    }, task_continuation_context::use_current());
}

void LibraryPage::OnNewShelf(Object^ sender, RoutedEventArgs^ e)
{
    LibraryPage^ self = this;
    AskName("New shelf", "", [self](const std::wstring& name)
    {
        Library::AddShelf(name);
        self->RebuildShelves(name);
    });
}

void LibraryPage::OnRenameShelf(Object^ sender, RoutedEventArgs^ e)
{
    auto from = Current();
    if (from.empty()) return;
    LibraryPage^ self = this;
    AskName("Rename shelf", ref new String(from.c_str()), [self, from](const std::wstring& to)
    {
        Library::RenameShelf(from, to);
        self->RebuildShelves(to);
    });
}

void LibraryPage::OnDeleteShelf(Object^ sender, RoutedEventArgs^ e)
{
    auto name = Current();
    if (name.empty()) return;
    auto dialog = ref new ContentDialog();
    dialog->Title = "Delete shelf?";
    dialog->Content = ref new String((L"“" + name + L"” will be removed. Its books stay in your library.").c_str());
    dialog->PrimaryButtonText = "Delete";
    dialog->SecondaryButtonText = "Cancel";
    LibraryPage^ self = this;
    create_task(dialog->ShowAsync()).then([self, name](ContentDialogResult r)
    {
        if (r != ContentDialogResult::Primary) return;
        Library::DeleteShelf(name);
        self->RebuildShelves(L"");
    }, task_continuation_context::use_current());
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
    auto shelf = Current();
    LibraryPage^ self = this;
    create_task(picker->PickMultipleFilesAsync()).then([shelf](IVectorView<StorageFile^>^ files)
    {
        auto chain = task_from_result();
        auto failed = std::make_shared<std::wstring>();
        auto ui = task_continuation_context::use_current();
        for (auto file : files)
        {
            chain = chain.then([file, failed, shelf]
            {
                return Library::Import(file).then([file, failed, shelf](task<Book^> t)
                {
                    try
                    {
                        auto book = t.get();
                        if (!shelf.empty() && !book->InShelf(shelf)) Library::SetShelf(book, shelf, true);
                    }
                    catch (Exception^) { *failed += std::wstring(file->Name->Data()) + L"\n"; }
                });
            }, ui);
        }
        return chain.then([failed]
        {
            if (!failed->empty())
                (ref new MessageDialog(ref new String(failed->c_str()), "Couldn't import"))->ShowAsync();
        }, ui);
    }).then([self](task<void> t)
    {
        try { t.get(); }
        catch (Exception^) {}
        self->Refresh();
    }, task_continuation_context::use_current());
}

void LibraryPage::ShowMenu(FrameworkElement^ target)
{
    auto book = dynamic_cast<Book^>(target->DataContext);
    if (!book) return;
    LibraryPage^ self = this;
    auto menu = ref new MenuFlyout();

    auto fav = ref new MenuFlyoutItem();
    fav->Text = book->Favorite ? "Remove from favourites" : "Add to favourites";
    fav->Click += ref new RoutedEventHandler([self, book](Object^, RoutedEventArgs^)
    {
        book->Favorite = !book->Favorite;
        Library::Save();
        self->Refresh();
    });
    menu->Items->Append(fav);

    auto shelves = ref new MenuFlyoutSubItem();
    shelves->Text = "Shelves";
    for (auto& name : Library::Shelves())
    {
        auto item = ref new ToggleMenuFlyoutItem();
        item->Text = ref new String(name.c_str());
        item->IsChecked = book->InShelf(name);
        std::wstring n = name;
        item->Click += ref new RoutedEventHandler([self, book, n](Object^ sender, RoutedEventArgs^)
        {
            Library::SetShelf(book, n, safe_cast<ToggleMenuFlyoutItem^>(sender)->IsChecked);
            self->Refresh();
        });
        shelves->Items->Append(item);
    }
    if (!Library::Shelves().empty()) shelves->Items->Append(ref new MenuFlyoutSeparator());
    auto add = ref new MenuFlyoutItem();
    add->Text = "New shelf…";
    add->Click += ref new RoutedEventHandler([self, book](Object^, RoutedEventArgs^)
    {
        self->AskName("New shelf", "", [self, book](const std::wstring& name)
        {
            Library::AddShelf(name);
            Library::SetShelf(book, name, true);
            self->RebuildShelves(self->Current());
        });
    });
    shelves->Items->Append(add);
    menu->Items->Append(shelves);

    menu->Items->Append(ref new MenuFlyoutSeparator());
    auto remove = ref new MenuFlyoutItem();
    remove->Text = "Remove";
    remove->Click += ref new RoutedEventHandler([book](Object^, RoutedEventArgs^) { Library::Remove(book); });
    menu->Items->Append(remove);
    menu->ShowAt(target);
}

void LibraryPage::OnHolding(Object^ sender, HoldingRoutedEventArgs^ e)
{
    if (e->HoldingState != Windows::UI::Input::HoldingState::Started) return;
    e->Handled = true;
    ShowMenu(safe_cast<FrameworkElement^>(sender));
}

void LibraryPage::OnRightTapped(Object^ sender, RightTappedRoutedEventArgs^ e)
{
    if (e->PointerDeviceType == Windows::Devices::Input::PointerDeviceType::Touch) return;
    e->Handled = true;
    ShowMenu(safe_cast<FrameworkElement^>(sender));
}
