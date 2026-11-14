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
        property Windows::UI::Xaml::Media::ImageSource^ Cover { Windows::UI::Xaml::Media::ImageSource^ get(); }

    private:
        Windows::UI::Xaml::Media::ImageSource^ cover;
    };

    class Library
    {
    public:
        static Platform::Collections::Vector<Book^>^ Books();
        static concurrency::task<void> Load();
        static void Save();
        static concurrency::task<Book^> Import(Windows::Storage::StorageFile^ file);
        static void Remove(Book^ book);
        static concurrency::task<Windows::Storage::StorageFile^> GetFile(Platform::String^ relativePath);
        static concurrency::task<Platform::String^> ReadText(Platform::String^ relativePath);
        static void WriteText(Platform::String^ folder, Platform::String^ name, Platform::String^ text);
    };
}
