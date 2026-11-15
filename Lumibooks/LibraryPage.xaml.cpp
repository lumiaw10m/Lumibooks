#include "pch.h"
#include "LibraryPage.xaml.h"
#include "FixedReaderPage.xaml.h"
#include "NotesPage.xaml.h"

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

static String^ SortKey(const std::wstring& shelf)
{
    return shelf.empty() ? ref new String(L"sort") : ref new String((L"sort/" + shelf).c_str());
}

int LibraryPage::SortOf(const std::wstring& shelf)
{
    auto values = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    for (auto key : { SortKey(shelf), SortKey(L"") })
        if (values->HasKey(key))
            if (auto v = dynamic_cast<Windows::Foundation::IPropertyValue^>(values->Lookup(key))) return v->GetInt32();
    return 0;
}

void LibraryPage::MoveSort(const std::wstring& from, const std::wstring& to)
{
    auto values = Windows::Storage::ApplicationData::Current->LocalSettings->Values;
    auto old = SortKey(from);
    if (!values->HasKey(old)) return;
    if (!to.empty()) values->Insert(SortKey(to), values->Lookup(old));
    values->Remove(old);
}

LibraryPage::LibraryPage() : building(false)
{
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

static std::wstring Lower(String^ s)
{
    std::wstring r = s ? s->Data() : L"";
    for (auto& c : r) c = towlower(c);
    return r;
}

void LibraryPage::OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    Refresh();
}

void LibraryPage::Refresh()
{
    if (building) return;
    auto& all = Library::Shelves();
    std::wstring query = SearchBox->Visibility == Windows::UI::Xaml::Visibility::Visible ? Lower(SearchBox->Text) : std::wstring();
    std::vector<Book^> order;
    for (auto b : Library::Books())
    {
        if (!query.empty() && Lower(b->Title).find(query) == std::wstring::npos && Lower(b->Author).find(query) == std::wstring::npos) continue;
        order.push_back(b);
    }
    for (size_t k = 0; k < views.size(); k++)
    {
        std::wstring shelf = k == 0 || k > all.size() ? std::wstring() : all[k - 1];
        auto& v = views[k];
        int mode = SortOf(shelf);
        std::vector<Book^> list;
        for (auto b : order)
            if (shelf.empty() || b->InShelf(shelf)) list.push_back(b);
        std::stable_sort(list.begin(), list.end(), [mode](Book^ a, Book^ b)
        {
            if (a->Favorite != b->Favorite) return a->Favorite;
            if (mode == 0) return a->Opened > b->Opened;
            if (mode == 1) return Lower(a->Title) < Lower(b->Title);
            if (mode == 2)
            {
                auto x = Lower(a->Author), y = Lower(b->Author);
                if (x.empty() != y.empty()) return y.empty();
                return x != y ? x < y : Lower(a->Title) < Lower(b->Title);
            }
            return false;
        });
        v.books->Clear();
        for (auto b : list) v.books->Append(b);
        if (mode == 1 || mode == 2)
        {
            auto groups = ref new Vector<BookGroup^>();
            BookGroup^ group = nullptr;
            std::wstring key;
            for (auto b : v.books)
            {
                std::wstring k;
                if (b->Favorite) k = L"★ Favourites";
                else if (mode == 2) k = b->Author && !b->Author->IsEmpty() ? std::wstring(b->Author->Data()) : L"Unknown author";
                else
                {
                    wchar_t c = b->Title && !b->Title->IsEmpty() ? towupper(b->Title->Data()[0]) : L'#';
                    k = iswalpha(c) ? std::wstring(1, c) : L"#";
                }
                if (!group || _wcsicmp(k.c_str(), key.c_str()) != 0)
                {
                    group = ref new BookGroup();
                    group->Title = ref new String(k.c_str());
                    groups->Append(group);
                    key = k;
                }
                group->Items->Append(b);
            }
            v.source->Source = groups;
            v.grid->ItemsSource = v.source->View;
        }
        else if (v.grid->ItemsSource != v.books) v.grid->ItemsSource = v.books;
        v.empty->Text = !Library::Books()->Size ? L"Your library is empty. Use Import to add books."
            : !query.empty() ? L"No books match your search."
            : L"This shelf is empty. Hold a book and choose Shelves to add it here.";
        v.empty->Visibility = v.books->Size ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    }
    if (continueCard)
    {
        Book^ last = nullptr;
        for (auto b : Library::Books())
            if (b->Opened > 0 && b->Percent < 99.5 && (!last || b->Opened > last->Opened)) last = b;
        continueCard->Content = last;
        continueCard->Visibility = last && query.empty() ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    }
    auto shelfButtons = Current().empty() ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible;
    RenameShelfButton->Visibility = shelfButtons;
    DeleteShelfButton->Visibility = shelfButtons;
}

void LibraryPage::RebuildShelves(const std::wstring& select)
{
    auto& all = Library::Shelves();
    building = true;
    Shelves->Items->Clear();
    views.clear();
    auto tpl = safe_cast<DataTemplate^>(Resources->Lookup("BookTemplate"));
    auto groupTpl = safe_cast<DataTemplate^>(Resources->Lookup("GroupTemplate"));
    int index = 0;
    for (size_t i = 0; i <= all.size(); i++)
    {
        ShelfView v;
        v.books = ref new Vector<Book^>();
        v.empty = ref new TextBlock();
        v.empty->TextWrapping = TextWrapping::Wrap;
        v.empty->Opacity = 0.7;
        v.empty->Margin = Thickness(0, 12, 0, 0);
        auto grid = ref new GridView();
        grid->ItemTemplate = tpl;
        grid->ItemsSource = v.books;
        grid->SelectionMode = ListViewSelectionMode::None;
        grid->IsItemClickEnabled = true;
        grid->ItemClick += ref new ItemClickEventHandler(this, &LibraryPage::OnItemClick);
        auto style = ref new GroupStyle();
        style->HeaderTemplate = groupTpl;
        grid->GroupStyle->Append(style);
        v.grid = grid;
        v.source = ref new Windows::UI::Xaml::Data::CollectionViewSource();
        v.source->IsSourceGrouped = true;
        v.source->ItemsPath = ref new PropertyPath("Items");
        auto host = ref new Grid();
        if (i == 0)
        {
            auto top = ref new RowDefinition();
            top->Height = GridLengthHelper::Auto;
            host->RowDefinitions->Append(top);
            host->RowDefinitions->Append(ref new RowDefinition());
            continueCard = ref new ContentControl();
            continueCard->ContentTemplate = safe_cast<DataTemplate^>(Resources->Lookup("ContinueTemplate"));
            continueCard->HorizontalContentAlignment = Windows::UI::Xaml::HorizontalAlignment::Stretch;
            continueCard->HorizontalAlignment = Windows::UI::Xaml::HorizontalAlignment::Stretch;
            continueCard->Margin = Thickness(4, 4, 0, 0);
            continueCard->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
            continueCard->Tapped += ref new TappedEventHandler(this, &LibraryPage::OnContinue);
            host->Children->Append(continueCard);
            Grid::SetRow(v.empty, 1);
            Grid::SetRow(grid, 1);
        }
        host->Children->Append(v.empty);
        host->Children->Append(grid);
        auto item = ref new PivotItem();
        item->Header = i == 0 ? ref new String(L"All") : ref new String(all[i - 1].c_str());
        item->Margin = Thickness(4, 0, 0, 0);
        item->Content = host;
        Shelves->Items->Append(item);
        views.push_back(v);
        if (i > 0 && all[i - 1] == select) index = (int)i;
    }
    Shelves->SelectedIndex = index;
    building = false;
    Refresh();
}

void LibraryPage::OnNotes(Object^ sender, RoutedEventArgs^ e)
{
    this->Frame->Navigate(TypeName(NotesPage::typeid));
}

void LibraryPage::OnContinue(Object^ sender, TappedRoutedEventArgs^ e)
{
    auto book = dynamic_cast<Book^>(continueCard->Content);
    if (book) this->Frame->Navigate(ReaderFor(book), book);
}

void LibraryPage::OnSearch(Object^ sender, RoutedEventArgs^ e)
{
    bool show = SearchBox->Visibility != Windows::UI::Xaml::Visibility::Visible;
    SearchBox->Visibility = show ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed;
    if (show) SearchBox->Focus(Windows::UI::Xaml::FocusState::Programmatic);
    else SearchBox->Text = "";
    Refresh();
}

void LibraryPage::OnSearchChanged(Object^ sender, TextChangedEventArgs^ e)
{
    Refresh();
}

void LibraryPage::OnSearchKey(Object^ sender, KeyRoutedEventArgs^ e)
{
    if (e->Key == Windows::System::VirtualKey::Enter) Shelves->Focus(Windows::UI::Xaml::FocusState::Programmatic);
}

void LibraryPage::OnSort(Object^ sender, RoutedEventArgs^ e)
{
    auto menu = ref new MenuFlyout();
    const wchar_t* names[] = { L"Recently read", L"Title", L"Author", L"Recently added" };
    LibraryPage^ self = this;
    std::wstring shelf = Current();
    int mode = SortOf(shelf);
    for (int i = 0; i < 4; i++)
    {
        auto item = ref new ToggleMenuFlyoutItem();
        item->Text = ref new String(names[i]);
        item->IsChecked = i == mode;
        item->Click += ref new RoutedEventHandler([self, i, shelf](Object^, RoutedEventArgs^)
        {
            Windows::Storage::ApplicationData::Current->LocalSettings->Values->Insert(SortKey(shelf), i);
            self->Refresh();
        });
        menu->Items->Append(item);
    }
    menu->ShowAt(safe_cast<FrameworkElement^>(sender));
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
        self->MoveSort(from, to);
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
        self->MoveSort(name, L"");
        self->RebuildShelves(L"");
    }, task_continuation_context::use_current());
}

void LibraryPage::OnItemClick(Object^ sender, ItemClickEventArgs^ e)
{
    auto book = safe_cast<Book^>(e->ClickedItem);
    this->Frame->Navigate(ReaderFor(book), book);
}

void LibraryPage::OnImport(Object^ sender, RoutedEventArgs^ e)
{
    auto picker = ref new FileOpenPicker();
    picker->SuggestedStartLocation = PickerLocationId::DocumentsLibrary;
    picker->ViewMode = PickerViewMode::List;
    for (int i = 0; i < ImportTypeCount; i++) picker->FileTypeFilter->Append(ref new String(ImportTypes[i]));
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
    add->Text = L"New shelf…";
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
