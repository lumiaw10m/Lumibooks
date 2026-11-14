#include "pch.h"
#include "Library.h"
#include "Settings.h"
#include "LibraryPage.xaml.h"
#include "ReaderPage.xaml.h"

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
}

Frame^ App::EnsureFrame()
{
    auto frame = dynamic_cast<Frame^>(Window::Current->Content);
    if (!frame)
    {
        Settings::Get().Load();
        Library::Load();
        frame = ref new Frame();
        frame->Navigated += ref new NavigatedEventHandler(this, &App::OnNavigated);
        SystemNavigationManager::GetForCurrentView()->BackRequested += ref new EventHandler<BackRequestedEventArgs^>(this, &App::OnBackRequested);
        Window::Current->Content = frame;
    }
    if (!frame->Content) frame->Navigate(TypeName(LibraryPage::typeid));
    return frame;
}

void App::OnLaunched(LaunchActivatedEventArgs^ e)
{
    EnsureFrame();
    Window::Current->Activate();
}

void App::OnFileActivated(FileActivatedEventArgs^ e)
{
    auto frame = EnsureFrame();
    Window::Current->Activate();
    if (!e->Files->Size) return;
    auto file = dynamic_cast<StorageFile^>(e->Files->GetAt(0));
    if (!file) return;
    Library::Import(file).then([frame](task<Book^> t)
    {
        try { frame->Navigate(TypeName(ReaderPage::typeid), t.get()); }
        catch (Exception^ ex) { (ref new MessageDialog(ex->Message, "Can't open book"))->ShowAsync(); }
    });
}

void App::OnBackRequested(Object^ sender, BackRequestedEventArgs^ e)
{
    auto frame = dynamic_cast<Frame^>(Window::Current->Content);
    ReaderPage^ reader = nullptr;
    if (frame) reader = dynamic_cast<ReaderPage^>(frame->Content);
    if (reader && reader->HandleBack())
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
