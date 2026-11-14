#include "pch.h"
#include "ReadAloud.h"
#include "Settings.h"

using namespace Lumibooks;
using namespace concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Media;
using namespace Windows::Media::Core;
using namespace Windows::Media::Playback;
using namespace Windows::Media::SpeechSynthesis;
using namespace Windows::Storage::Streams;
using namespace Windows::UI::Core;

static const int Ahead = 3;

ReadAloud::ReadAloud(Book^ book, std::function<void(int)> onSentence, std::function<void()> onEnd)
    : book(book), onSentence(onSentence), onEnd(onEnd), cur(0), gen(0), busy(false), waiting(false), active(false)
{
    synth = ref new SpeechSynthesizer();
    player = ref new MediaPlayer();
    player->AudioCategory = MediaPlayerAudioCategory::Media;
    auto commands = player->CommandManager;
    commands->NextBehavior->EnablingRule = MediaCommandEnablingRule::Always;
    commands->PreviousBehavior->EnablingRule = MediaCommandEnablingRule::Always;

    auto ui = CoreWindow::GetForCurrentThread()->Dispatcher;
    WeakReference weak(this);
    auto skip = [ui, weak](int delta)
    {
        ui->RunAsync(CoreDispatcherPriority::Normal, ref new DispatchedHandler([weak, delta]()
        {
            auto self = weak.Resolve<ReadAloud>();
            if (self) self->Skip(delta);
        }));
    };
    player->MediaEnded += ref new TypedEventHandler<MediaPlayer^, Object^>([skip](MediaPlayer^, Object^) { skip(1); });
    player->MediaFailed += ref new TypedEventHandler<MediaPlayer^, MediaPlayerFailedEventArgs^>([skip](MediaPlayer^, MediaPlayerFailedEventArgs^) { skip(1); });
    commands->NextReceived += ref new TypedEventHandler<MediaPlaybackCommandManager^, MediaPlaybackCommandManagerNextReceivedEventArgs^>(
        [skip](MediaPlaybackCommandManager^, MediaPlaybackCommandManagerNextReceivedEventArgs^ e) { e->Handled = true; skip(1); });
    commands->PreviousReceived += ref new TypedEventHandler<MediaPlaybackCommandManager^, MediaPlaybackCommandManagerPreviousReceivedEventArgs^>(
        [skip](MediaPlaybackCommandManager^, MediaPlaybackCommandManagerPreviousReceivedEventArgs^ e) { e->Handled = true; skip(-1); });
}

void ReadAloud::ApplyVoice()
{
    const std::wstring& id = Settings::Get().voice;
    VoiceInformation^ voice = SpeechSynthesizer::DefaultVoice;
    for (auto v : SpeechSynthesizer::AllVoices)
        if (id == v->Id->Data()) voice = v;
    try { synth->Voice = voice; }
    catch (Exception^) {}
}

void ReadAloud::Start(std::vector<std::wstring>&& list, int from)
{
    sentences = std::move(list);
    cache.clear();
    gen++;
    cur = from < 0 ? 0 : from;
    active = true;
    ApplyVoice();
    PlayCurrent();
}

void ReadAloud::Restart()
{
    if (!active) return;
    cache.clear();
    gen++;
    player->Pause();
    ApplyVoice();
    PlayCurrent();
}

void ReadAloud::Stop()
{
    active = false;
    waiting = false;
    gen++;
    cache.clear();
    player->Pause();
    player->Source = nullptr;
}

void ReadAloud::Skip(int delta)
{
    if (!active) return;
    cur += delta;
    if (cur < 0) cur = 0;
    PlayCurrent();
}

String^ ReadAloud::Ssml(const std::wstring& text)
{
    std::wstring body;
    for (wchar_t c : text)
    {
        if (c == L'&') body += L"&amp;";
        else if (c == L'<') body += L"&lt;";
        else if (c == L'>') body += L"&gt;";
        else body += c < 0x20 ? L' ' : c;
    }
    auto& s = Settings::Get();
    wchar_t head[256];
    swprintf_s(head, L"<speak version='1.0' xmlns='http://www.w3.org/2001/10/synthesis' xml:lang='%s'><prosody rate='%+d%%' pitch='%+d%%'>",
        synth->Voice->Language->Data(), s.rate, s.pitch);
    return ref new String((head + body + L"</prosody></speak>").c_str());
}

void ReadAloud::Pump()
{
    if (busy || !active) return;
    int i = -1;
    for (int k = cur; k < cur + Ahead && k < (int)sentences.size(); k++)
        if (!cache.count(k)) { i = k; break; }
    if (i < 0) return;
    busy = true;
    int g = gen;
    ReadAloud^ self = this;
    String^ text = ref new String(sentences[i].c_str());
    create_task(synth->SynthesizeSsmlToStreamAsync(Ssml(sentences[i]))).then([self, text](task<SpeechSynthesisStream^> t) -> task<SpeechSynthesisStream^>
    {
        try { return task_from_result(t.get()); }
        catch (Exception^) { return create_task(self->synth->SynthesizeTextToStreamAsync(text)); }
    }).then([self, i, g](task<SpeechSynthesisStream^> t)
    {
        self->busy = false;
        SpeechSynthesisStream^ stream = nullptr;
        try { stream = t.get(); }
        catch (Exception^) {}
        if (g == self->gen)
        {
            self->cache[i] = stream;
            if (self->waiting && i == self->cur) self->PlayCurrent();
        }
        self->Pump();
    });
}

void ReadAloud::PlayCurrent()
{
    if (!active) return;
    if (cur >= (int)sentences.size())
    {
        Stop();
        onEnd();
        return;
    }
    auto it = cache.find(cur);
    if (it == cache.end())
    {
        waiting = true;
        Pump();
        return;
    }
    waiting = false;
    auto stream = it->second;
    if (!stream)
    {
        cur++;
        PlayCurrent();
        return;
    }
    auto item = ref new MediaPlaybackItem(MediaSource::CreateFromStream(stream, stream->ContentType));
    auto props = item->GetDisplayProperties();
    props->Type = MediaPlaybackType::Music;
    props->MusicProperties->Title = book->Title;
    props->MusicProperties->Artist = book->Author;
    if (book->CoverPath && !book->CoverPath->IsEmpty())
        props->Thumbnail = RandomAccessStreamReference::CreateFromUri(ref new Uri("ms-appdata:///local/" + book->CoverPath));
    item->ApplyDisplayProperties(props);
    player->Source = item;
    player->Play();
    for (auto c = cache.begin(); c != cache.end();)
        c = c->first < cur ? cache.erase(c) : std::next(c);
    onSentence(cur);
    Pump();
}
