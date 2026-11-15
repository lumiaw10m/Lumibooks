#pragma once
#include <map>

namespace Lumibooks
{
    struct BookStats
    {
        double seconds = 0, first = 0, last = 0, finished = 0;
        int pages = 0, sessions = 0;
    };

    struct DayStats
    {
        double seconds = 0;
        int pages = 0;
    };

    class Stats
    {
    public:
        static concurrency::task<void> Load();
        static void Begin(Platform::String^ id);
        static void Turn();
        static void End();
        static void Finish(Platform::String^ id);
        static void Forget(Platform::String^ id);
        static void Save();
        static BookStats Book(Platform::String^ id);
        static const std::map<std::wstring, DayStats>& Days();
        static std::wstring Today(int offset = 0);
        static double Now();
    };
}
