#include "pch.h"
#include "Library.h"
#include "Formats.h"
#include "Stats.h"

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
static std::vector<std::wstring> s_shelves;

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
        JsonObject^ root = nullptr;
        if (!text) return;
        if (JsonObject::TryParse(text, &root))
        {
            arr = root->GetNamedArray("books", ref new JsonArray());
            auto sh = root->GetNamedArray("shelves", ref new JsonArray());
            for (unsigned int i = 0; i < sh->Size; i++)
                if (sh->GetAt(i)->ValueType == JsonValueType::String) s_shelves.push_back(sh->GetStringAt(i)->Data());
        }
        else if (!JsonArray::TryParse(text, &arr)) return;
        for (unsigned int i = 0; i < arr->Size; i++)
        {
            if (arr->GetAt(i)->ValueType != JsonValueType::Object) continue;
            auto o = arr->GetObjectAt(i);
            auto b = ref new Book();
            b->Id = o->GetNamedString("id", "");
            b->Title = o->GetNamedString("title", "");
            b->Author = o->GetNamedString("author", "");
            b->FilePath = o->GetNamedString("file", "");
            b->CoverPath = o->GetNamedString("cover", "");
            b->ChapterIndex = (int)o->GetNamedNumber("chapter", 0);
            b->ChapterProgress = o->GetNamedNumber("progress", 0);
            b->Favorite = o->GetNamedBoolean("fav", false);
            b->Percent = o->GetNamedNumber("pct", 0);
            b->Opened = o->GetNamedNumber("opened", 0);
            auto sh = o->GetNamedArray("shelves", ref new JsonArray());
            for (unsigned int j = 0; j < sh->Size; j++)
                if (sh->GetAt(j)->ValueType == JsonValueType::String) b->shelves.push_back(sh->GetStringAt(j)->Data());
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
        if (b->Favorite) o->Insert("fav", JsonValue::CreateBooleanValue(true));
        if (b->Percent > 0) o->Insert("pct", JsonValue::CreateNumberValue(b->Percent));
        if (b->Opened > 0) o->Insert("opened", JsonValue::CreateNumberValue(b->Opened));
        if (!b->shelves.empty())
        {
            auto sh = ref new JsonArray();
            for (auto& n : b->shelves) sh->Append(JsonValue::CreateStringValue(Str(n)));
            o->Insert("shelves", sh);
        }
        arr->Append(o);
    }
    auto sh = ref new JsonArray();
    for (auto& n : s_shelves) sh->Append(JsonValue::CreateStringValue(Str(n)));
    auto root = ref new JsonObject();
    root->Insert("books", arr);
    root->Insert("shelves", sh);
    WriteText("", "library.json", root->Stringify());
}

static task<void> WriteIn(StorageFolder^ dir, String^ name, String^ text)
{
    return create_task(dir->CreateFileAsync(name + ".tmp", CreationCollisionOption::ReplaceExisting)).then([name, text](StorageFile^ f)
    {
        return create_task(FileIO::WriteTextAsync(f, text)).then([f, name]
        {
            return f->RenameAsync(name, NameCollisionOption::ReplaceExisting);
        });
    });
}

void Library::WriteText(String^ folder, String^ name, String^ text)
{
    Load();
    auto local = ApplicationData::Current->LocalFolder;
    s_chain = s_chain.then([local, folder, name, text]() -> task<void>
    {
        if (!folder || folder->IsEmpty()) return WriteIn(local, name, text);
        return create_task(local->CreateFolderAsync(folder, CreationCollisionOption::OpenIfExists)).then([name, text](StorageFolder^ dir)
        {
            return WriteIn(dir, name, text);
        });
    }).then([](task<void> t) { Swallow(t); });
}

task<String^> Library::ReadText(String^ relativePath)
{
    return GetFile(relativePath).then([](StorageFile^ f)
    {
        return FileIO::ReadTextAsync(f);
    }).then([](task<String^> t) -> String^
    {
        try { return t.get(); }
        catch (Exception^) { return nullptr; }
    });
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
    auto ui = task_continuation_context::use_current();
    std::wstring base = file->Name->Data();
    size_t dot = base.rfind(L'.');
    if (dot != std::wstring::npos && dot > 0) base.resize(dot);
    String^ name = Str(base);
    String^ id = NewId();
    Format fmt = FormatOf(file->Name->Data());
    String^ fileName = id + ref new String(Extension(fmt));
    return Load().then([file]
    {
        return FileIO::ReadBufferAsync(file);
    }, ui).then([=](IBuffer^ buf) -> task<Book^>
    {
        if (fmt == Format::Unknown) throw ref new FailureException("This file type isn't supported.");
        std::wstring t, a;
        std::vector<uint8_t> data;
        std::string ext = ".png";
        if (IsFixed(fmt))
        {
            bool locked = false;
            auto doc = FixedDoc::Open(FromBuffer(buf), fmt, &locked);
            if (!doc) throw ref new FailureException(locked ? "Password-protected documents aren't supported yet." : "This document can't be opened.");
            t = doc->title;
            a = doc->author;
            doc->Cover(208, data);
        }
        else
        {
            auto epub = OpenReflow(FromBuffer(buf), fmt);
            if (!epub)
            {
                String^ why = fmt == Format::Mobi ? "This MOBI book can't be opened. Only DRM-free MOBI files with standard compression are supported." : "This book can't be opened.";
                if (fmt == Format::Mobi && *mu_error()) why += " (" + Str(Wide(mu_error(), strlen(mu_error()))) + ")";
                throw ref new FailureException(why);
            }
            t = epub->title;
            a = epub->author;
            if (!epub->coverPath.empty() && epub->Read(epub->coverPath, data) && !data.empty())
            {
                std::string p = epub->coverPath;
                size_t dot = p.rfind('.');
                ext = dot == std::string::npos ? ".jpg" : p.substr(dot);
                for (auto& c : ext) c = (char)tolower((unsigned char)c);
            }
            else data.clear();
        }
        if (_wcsicmp(t.c_str(), file->Name->Data()) == 0) t.clear();
        String^ title = t.empty() ? name : Str(t);
        String^ author = Str(a);
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
        book->FilePath = "books/" + fileName;
        book->CoverPath = "";
        IBuffer^ cover = nullptr;
        String^ coverName = nullptr;
        if (!data.empty())
        {
            coverName = id + Str(Wide(ext.c_str(), ext.size()));
            book->CoverPath = "covers/" + coverName;
            cover = ToBuffer(data.data(), data.size());
        }
        return create_task(local->CreateFolderAsync("books", CreationCollisionOption::OpenIfExists))
            .then([file, fileName](StorageFolder^ dir)
        {
            return file->CopyAsync(dir, fileName, NameCollisionOption::ReplaceExisting);
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
        }, ui);
    }, ui);
}

void Library::Remove(Book^ book)
{
    unsigned int i;
    if (Books()->IndexOf(book, &i)) Books()->RemoveAt(i);
    Stats::Forget(book->Id);
    Save();
    for (auto p : { book->FilePath, book->CoverPath, "notes/" + book->Id + ".json" })
    {
        if (!p || p->IsEmpty()) continue;
        GetFile(p).then([](StorageFile^ f) { return f->DeleteAsync(); }).then([](task<void> t) { Swallow(t); });
    }
}

std::vector<std::wstring>& Library::Shelves()
{
    return s_shelves;
}

void Library::AddShelf(const std::wstring& name)
{
    if (name.empty() || std::find(s_shelves.begin(), s_shelves.end(), name) != s_shelves.end()) return;
    s_shelves.push_back(name);
    Save();
}

void Library::RenameShelf(const std::wstring& from, const std::wstring& to)
{
    if (to.empty() || from == to || std::find(s_shelves.begin(), s_shelves.end(), to) != s_shelves.end()) return;
    std::replace(s_shelves.begin(), s_shelves.end(), from, to);
    for (auto b : Books()) std::replace(b->shelves.begin(), b->shelves.end(), from, to);
    Save();
}

void Library::DeleteShelf(const std::wstring& name)
{
    s_shelves.erase(std::remove(s_shelves.begin(), s_shelves.end(), name), s_shelves.end());
    for (auto b : Books()) b->shelves.erase(std::remove(b->shelves.begin(), b->shelves.end(), name), b->shelves.end());
    Save();
}

void Library::SetShelf(Book^ book, const std::wstring& name, bool on)
{
    auto& v = book->shelves;
    v.erase(std::remove(v.begin(), v.end(), name), v.end());
    if (on) v.push_back(name);
    Save();
}
