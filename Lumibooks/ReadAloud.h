#pragma once
#include <functional>
#include <map>
#include "Library.h"

namespace Lumibooks
{
    ref class ReadAloud sealed
    {
    internal:
        ReadAloud(Book^ book, std::function<void(int)> onSentence, std::function<void()> onEnd);
        void Start(std::vector<std::wstring>&& sentences, int from);
        void Restart();
        void Stop();
        bool IsActive() { return active; }

    private:
        void ApplyVoice();
        void Pump();
        void PlayCurrent();
        void Skip(int delta);
        Platform::String^ Ssml(const std::wstring& text);

        Book^ book;
        Windows::Media::Playback::MediaPlayer^ player;
        Windows::Media::SpeechSynthesis::SpeechSynthesizer^ synth;
        std::function<void(int)> onSentence;
        std::function<void()> onEnd;
        std::vector<std::wstring> sentences;
        std::map<int, Windows::Media::SpeechSynthesis::SpeechSynthesisStream^> cache;
        int cur, gen;
        bool busy, waiting, active;
    };
}
