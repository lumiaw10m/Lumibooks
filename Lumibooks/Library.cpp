#include "pch.h"
#include "Library.h"
#include "Epub.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Platform::Collections;
using namespace Windows::Data::Json;
using namespace Windows::Foundation;
using namespace Windows::Storage;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;

static Vector<Book^>^ s_books;
static task<void> s_chain;
static bool s_loaded;

ImageSource^ Book::Cover::get()
{
    if (!cover && CoverPath && !CoverPath->IsEmpty())
    {
        auto bmp = ref new BitmapImage();
        bmp->DecodePixelType = DecodePixelType::Logical;
        bmp->DecodePixelWidth = 104;
        bmp->UriSource = ref new Uri("ms-appdata:///local/" + CoverPath);
        cover = bmp;
    }
    return cover;
}

Vector<Book^>^ Library::Books()
{
    if (!s_books) s_books = ref new Vector<Book^>();
    return s_books;
}

static String^ Str(const std::wstring& s)
{
    return ref new String(s.c_str(), (unsigned int)s.size());
}

static void Swallow(task<void> t)
{
    try { t.get(); }
    catch (Exception^) {}
}

task<StorageFile^> Library::GetFile(String^ relativePath)
{
    std::wstring p = ApplicationData::Current->LocalFolder->Path->Data();
    p += L'\\';
    for (auto c : std::wstring(relativePath->Data())) p += c == L'/' ? L'\\' : c;
    return create_task(StorageFile::GetFileFromPathAsync(Str(p)));
}

task<void> Library::Load()
{
    if (s_loaded) return s_chain;
    s_loaded = true;
    auto books = Books();
    s_chain = create_task(ApplicationData::Current->LocalFolder->TryGetItemAsync("library.json"))
        .then([](IStorageItem^ item) -> task<String^>
    {
        auto f = dynamic_cast<StorageFile^>(item);
        if (!f) return task_from_result<String^>(nullptr);
        return create_task(FileIO::ReadTextAsync(f));
    }).then([books](task<String^> t)
    {
        String^ text = nullptr;
        try { text = t.get(); }
        catch (Exception^) {}
        JsonArray^ arr = nullptr;
        if (!text || !JsonArray::TryParse(text, &arr)) return;
        for (unsigned int i = 0; i < arr->Size; i++)
        {
            auto o = dynamic_cast<JsonObject^>(arr->GetAt(i));
            if (!o) continue;
            auto b = ref new Book();
            b->Id = o->GetNamedString("id", "");
            b->Title = o->GetNamedString("title", "");
            b->Author = o->GetNamedString("author", "");
            b->FilePath = o->GetNamedString("file", "");
            b->CoverPath = o->GetNamedString("cover", "");
            b->ChapterIndex = (int)o->GetNamedNumber("chapter", 0);
            b->ChapterProgress = o->GetNamedNumber("progress", 0);
            if (!b->FilePath->IsEmpty()) books->Append(b);
        }
    });
    return s_chain;
}

void Library::Save()
{
    Load();
    auto arr = ref new JsonArray();
    for (unsigned int i = 0; i < Books()->Size; i++)
    {
        auto b = Books()->GetAt(i);
        auto o = ref new JsonObject();
        o->Insert("id", JsonValue::CreateStringValue(b->Id));
        o->Insert("title", JsonValue::CreateStringValue(b->Title));
        o->Insert("author", JsonValue::CreateStringValue(b->Author));
        o->Insert("file", JsonValue::CreateStringValue(b->FilePath));
        o->Insert("cover", JsonValue::CreateStringValue(b->CoverPath));
        o->Insert("chapter", JsonValue::CreateNumberValue(b->ChapterIndex));
        o->Insert("progress", JsonValue::CreateNumberValue(b->ChapterProgress));
        arr->Append(o);
    }
    String^ text = arr->Stringify();
    auto local = ApplicationData::Current->LocalFolder;
    s_chain = s_chain.then([local, text]
    {
        return create_task(local->CreateFileAsync("library.tmp", CreationCollisionOption::ReplaceExisting))
            .then([text](StorageFile^ f)
        {
            return create_task(FileIO::WriteTextAsync(f, text)).then([f]
            {
                return f->RenameAsync("library.json", NameCollisionOption::ReplaceExisting);
            });
        });
    }).then([](task<void> t) { Swallow(t); });
}

static String^ NewId()
{
    GUID g;
    CoCreateGuid(&g);
    wchar_t s[40];
    StringFromGUID2(g, s, 40);
    std::wstring r;
    for (wchar_t* p = s; *p; p++) if (iswxdigit(*p)) r += towlower(*p);
    return Str(r);
}

task<Book^> Library::Import(StorageFile^ file)
{
    auto local = ApplicationData::Current->LocalFolder;
    String^ name = file->DisplayName;
    String^ id = NewId();
    return Load().then([file]
    {
        return FileIO::ReadBufferAsync(file);
    }).then([=](IBuffer^ buf) -> task<Book^>
    {
        auto epub = Epub::Open(FromBuffer(buf));
        if (!epub) throw ref new FailureException("This file is not a readable EPUB.");
        String^ title = epub->title.empty() ? name : Str(epub->title);
        String^ author = Str(epub->author);
        for (unsigned int i = 0; i < Books()->Size; i++)
        {
            auto b = Books()->GetAt(i);
            if (String::CompareOrdinal(b->Title, title) == 0 && String::CompareOrdinal(b->Author, author) == 0)
                return task_from_result(b);
        }
        auto book = ref new Book();
        book->Id = id;
        book->Title = title;
        book->Author = author;
        book->FilePath = "books/" + id + ".epub";
        book->CoverPath = "";
        IBuffer^ cover = nullptr;
        String^ coverName = nullptr;
        std::vector<uint8_t> data;
        if (!epub->coverPath.empty() && epub->Read(epub->coverPath, data) && !data.empty())
        {
            std::string p = epub->coverPath;
            size_t dot = p.rfind('.');
            std::string ext = dot == std::string::npos ? ".jpg" : p.substr(dot);
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            coverName = id + Str(Wide(ext.c_str(), ext.size()));
            book->CoverPath = "covers/" + coverName;
            cover = ToBuffer(data.data(), data.size());
        }
        return create_task(local->CreateFolderAsync("books", CreationCollisionOption::OpenIfExists))
            .then([file, id](StorageFolder^ dir)
        {
            return file->CopyAsync(dir, id + ".epub", NameCollisionOption::ReplaceExisting);
        }).then([local, cover, coverName](StorageFile^) -> task<void>
        {
            if (!cover) return task_from_result();
            return create_task(local->CreateFolderAsync("covers", CreationCollisionOption::OpenIfExists))
                .then([coverName](StorageFolder^ dir)
            {
                return dir->CreateFileAsync(coverName, CreationCollisionOption::ReplaceExisting);
            }).then([cover](StorageFile^ f)
            {
                return FileIO::WriteBufferAsync(f, cover);
            });
        }).then([book]
        {
            Books()->InsertAt(0, book);
            Save();
            return book;
        });
    });
}

void Library::Remove(Book^ book)
{
    unsigned int i;
    if (Books()->IndexOf(book, &i)) Books()->RemoveAt(i);
    Save();
    for (auto p : { book->FilePath, book->CoverPath })
    {
        if (!p || p->IsEmpty()) continue;
        GetFile(p).then([](StorageFile^ f) { return f->DeleteAsync(); }).then([](task<void> t) { Swallow(t); });
    }
}
