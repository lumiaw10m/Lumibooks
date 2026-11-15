#pragma once

namespace Lumibooks
{
    public ref class Book sealed
    {
    public:
        property Platform::String^ Id;
        property Platform::String^ Title;
        property Platform::String^ Author;
        property Platform::String^ FilePath;
        property Platform::String^ CoverPath;
        property int ChapterIndex;
        property double ChapterProgress;
        property bool Favorite;
        property double Percent;
        property double Opened;
        property Windows::UI::Xaml::Visibility NoCoverVisibility
        {
            Windows::UI::Xaml::Visibility get() { return CoverPath && !CoverPath->IsEmpty() ? Windows::UI::Xaml::Visibility::Collapsed : Windows::UI::Xaml::Visibility::Visible; }
        }
        property Windows::UI::Xaml::Visibility ProgressVisibility
        {
            Windows::UI::Xaml::Visibility get() { return Percent > 0 ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed; }
        }
        property Platform::String^ PercentText
        {
            Platform::String^ get()
            {
                wchar_t s[16];
                swprintf_s(s, L"%d%%", (int)(Percent + 0.5));
                return Percent > 0 ? ref new Platform::String(s) : nullptr;
            }
        }
        property Windows::UI::Xaml::Media::ImageSource^ Cover { Windows::UI::Xaml::Media::ImageSource^ get(); }
        property Windows::UI::Xaml::Visibility FavoriteVisibility
        {
            Windows::UI::Xaml::Visibility get() { return Favorite ? Windows::UI::Xaml::Visibility::Visible : Windows::UI::Xaml::Visibility::Collapsed; }
        }

    internal:
        std::vector<std::wstring> shelves;
        std::wstring jump;
        bool InShelf(const std::wstring& name) { return std::find(shelves.begin(), shelves.end(), name) != shelves.end(); }

    private:
        Windows::UI::Xaml::Media::ImageSource^ cover;
    };

    [Windows::UI::Xaml::Data::Bindable]
    public ref class BookGroup sealed
    {
    public:
        BookGroup() { Items = ref new Platform::Collections::Vector<Book^>(); }
        property Platform::String^ Title;
        property Windows::Foundation::Collections::IObservableVector<Book^>^ Items;
    };

    class Library
    {
    public:
        static Platform::Collections::Vector<Book^>^ Books();
        static concurrency::task<void> Load();
        static void Save();
        static concurrency::task<Book^> Import(Windows::Storage::StorageFile^ file);
        static void Remove(Book^ book);
        static std::vector<std::wstring>& Shelves();
        static void AddShelf(const std::wstring& name);
        static void RenameShelf(const std::wstring& from, const std::wstring& to);
        static void DeleteShelf(const std::wstring& name);
        static void SetShelf(Book^ book, const std::wstring& name, bool on);
        static concurrency::task<Windows::Storage::StorageFile^> GetFile(Platform::String^ relativePath);
        static concurrency::task<Platform::String^> ReadText(Platform::String^ relativePath);
        static void WriteText(Platform::String^ folder, Platform::String^ name, Platform::String^ text);
    };
}
