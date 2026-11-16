# Lumibooks

![Lumibooks on two Lumia 950s: the library and the reader](docs/hero.webp)

A light, native eBook reader for Windows 10 Mobile, written in C++/CX.

- Targets Windows 10 Mobile 15063 (Lumia 950) and desktop x86
- EPUB, FB2 and MOBI in a reflowing reader; PDF, XPS and CBZ rendered with MuPDF
- Library with covers, progress, shelves, favourites, search and sorting by recent, title or author
- Read aloud using the system voices, with sentence highlighting and lock-screen controls
- Fonts, size, weight, letter and line spacing, margins, paragraph spacing, indent, hyphenation
- Day, sepia, night, black and custom color themes, plus a night light filter
- Word selection with highlights in four colors, notes, copy, share, define and translate
- Footnote pop-ups, search inside books, a table of contents with page numbers and bookmarks
- Reading statistics per book and per day

## Installing

Download the files from the latest [release](https://github.com/lumiaw10m/Lumibooks/releases/latest). On the phone, turn on developer mode (Settings > Update & security > For developers), then install the `.appx` together with the `Microsoft.VCLibs.ARM.14.00.appx` dependency through Device Portal. If Windows doesn't trust the package, install `Lumibooks.cer` into Trusted People first.

## Building

You'll need Visual Studio 2019 with the UWP workload, the C++ UWP ARM tools and the Windows 10 SDK 19041.

```powershell
powershell -ExecutionPolicy Bypass -File Lumibooks\build.ps1
```

This builds a Release|ARM `.appx` (no bundle) into `Lumibooks\out` and bumps the package version first. The first run creates a `CN=lumiaw10m` signing certificate (`Lumibooks.pfx`, git-ignored, and `Lumibooks.cer`). Add `-Install` to install the package on the phone through Device Portal:

```powershell
$env:LUMIA_IP = '192.168.1.50'
powershell -ExecutionPolicy Bypass -File Lumibooks\build.ps1 -Install
```

Use `-Platform x86` for a desktop build, or open `Lumibooks.sln` in Visual Studio to debug.

## Privacy

Lumibooks keeps your books, notes and statistics on the phone. The only time it goes online is when you choose Define or Translate: the selected text is then sent to [Wiktionary](https://en.wiktionary.org) or [MyMemory](https://mymemory.translated.net) to look it up.

## License

Lumibooks is licensed under the GNU Affero General Public License v3.0, the same license as MuPDF. See [LICENSE](LICENSE).

## Third-party software

- [MuPDF](https://mupdf.com) 1.28.6, AGPL-3.0. Built without its HTML engine, JavaScript, OCR and extra fonts. See `Lumibooks/ThirdParty/mupdf/COPYING`.
- [FreeType](https://freetype.org) 2.14.3, FreeType License. Portions of this software are copyright © 2026 The FreeType Project (www.freetype.org). All rights reserved.
- [libjpeg](https://ijg.org), IJG License. This software is based in part on the work of the Independent JPEG Group.
- [jbig2dec](https://jbig2dec.com), AGPL-3.0.
- [OpenJPEG](https://www.openjpeg.org), BSD 2-clause.
- [zlib](https://zlib.net), zlib License.
- [miniz](https://github.com/richgel999/miniz), MIT.
- The URW base 14 PDF fonts that ship with MuPDF, AGPL-3.0.

Each library's license text is included next to its source in `Lumibooks/ThirdParty`.
