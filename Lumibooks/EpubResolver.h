#pragma once
#include "Epub.h"

namespace Lumibooks
{
    ref class EpubResolver sealed : Windows::Web::IUriToStreamResolver
    {
    public:
        virtual Windows::Foundation::IAsyncOperation<Windows::Storage::Streams::IInputStream^>^ UriToStreamAsync(Windows::Foundation::Uri^ uri);

    internal:
        EpubResolver(std::shared_ptr<Epub> epub) : epub(epub) {}
        static std::string PathOf(Windows::Foundation::Uri^ uri);

    private:
        std::shared_ptr<Epub> epub;
    };
}
