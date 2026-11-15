#include "pch.h"
#include "Library.h"
#include "Settings.h"
#include "Stats.h"
#include "LibraryPage.xaml.h"
#include "ReaderPage.xaml.h"
#include "FixedReaderPage.xaml.h"
#include "NotesPage.xaml.h"
#include "StatsPage.xaml.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::UI::Core;
using namespace Windows::UI::Popups;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Interop;
using namespace Windows::UI::Xaml::Navigation;

App::App()
{
    InitializeComponent();
    Suspending += ref new SuspendingEventHandler([](Object^, Windows::ApplicationModel::SuspendingEventArgs^) { Stats::Save(); });
}

Frame^ App::EnsureFrame()
{
    auto frame = dynamic_cast<Frame^>(Window::Current->Content);
    if (!frame)
    {
        Settings::Get().Load();
        Stats::Load();
        Library::Load();
        frame = ref new Frame();
        frame->Navigated += ref new NavigatedEventHandler(this, &App::OnNavigated);
        SystemNavigationManager::GetForCurrentView()->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &App::OnBackRequested);
        Window::Current->Content = frame;
    }
    if (!frame->Content) frame->Navigate(TypeName(LibraryPage::typeid));
    return frame;
}

static void ImportAndOpen(Frame^ frame, task<StorageFile^> file)
{
    file.then([](StorageFile^ f) { return Library::Import(f); }).then([frame](task<Book^> t)
    {
        try { auto b = t.get(); frame->Navigate(ReaderFor(b), b); }
        catch (Exception^ ex) { (ref new MessageDialog(ex->Message, "Can't open book"))->ShowAsync(); }
    });
}

void App::OnLaunched(LaunchActivatedEventArgs^ e)
{
    std::wstring arg = e->Arguments ? e->Arguments->Data() : L"";
    if (arg.compare(0, 5, L"sort:") == 0) ApplicationData::Current->LocalSettings->Values->Insert("sort", _wtoi(arg.c_str() + 5));
    auto frame = EnsureFrame();
    Window::Current->Activate();
    if (arg.compare(0, 7, L"import:") == 0)
    {
        auto path = ApplicationData::Current->LocalFolder->Path + "\\inbox\\" +ref new String(arg.c_str() + 7);
        ImportAndOpen(frame, create_task(StorageFile::GetFileFromPathAsync(path)));
    }
    else if (arg == L"stats") Library::Load().then([frame] { frame->Navigate(TypeName(StatsPage::typeid)); }, task_continuation_context::use_current());
    else if (arg == L"notes") Library::Load().then([frame] { frame->Navigate(TypeName(NotesPage::typeid)); }, task_continuation_context::use_current());
}

void App::OnFileActivated(FileActivatedEventArgs^ e)
{
    auto frame = EnsureFrame();
    Window::Current->Activate();
    if (!e->Files->Size) return;
    auto file = dynamic_cast<StorageFile^>(e->Files->GetAt(0));
    if (file) ImportAndOpen(frame, task_from_result(file));
}

void App::OnBackRequested(Object^ sender, BackRequestedEventArgs^ e)
{
    auto frame = dynamic_cast<Frame^>(Window::Current->Content);
    ReaderPage^ reader = nullptr;
    if (frame) reader = dynamic_cast<ReaderPage^>(frame->Content);
    auto fixed = frame ? dynamic_cast<FixedReaderPage^>(frame->Content) : nullptr;
    if ((reader && reader->HandleBack()) || (fixed && fixed->HandleBack()))
    {
        e->Handled = true;
        return;
    }
    if (frame && frame->CanGoBack)
    {
        e->Handled = true;
        frame->GoBack();
    }
}

void App::OnNavigated(Object^ sender, NavigationEventArgs^ e)
{
    SystemNavigationManager::GetForCurrentView()->AppViewBackButtonVisibility =
        safe_cast<Frame^>(sender)->CanGoBack ? AppViewBackButtonVisibility::Visible : AppViewBackButtonVisibility::Collapsed;
}
