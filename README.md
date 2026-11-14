# Lumibooks

A light, native eBook reader for Windows 10 Mobile, written in C++/CX.

- Targets Windows 10 Mobile 15063 (Lumia 950) and desktop x86
- EPUB first; PDF, CBZ, MOBI and FB2 to follow through MuPDF
- Read aloud using the system voices, with sentence highlighting and lock-screen controls
- Fonts, size, weight, letter and line spacing, margins, paragraph spacing, indent, hyphenation
- Day, sepia, night, black and custom color themes, plus a night light filter
- Highlights in four colors, notes, bookmarks and a table of contents

## Building

Open `Lumibooks.sln` in Visual Studio 2019 with the C++ Universal Windows Platform tools (including the ARM tools). Select `ARM`, then deploy to a developer-unlocked device. Use `x86` to run on the desktop.

The project targets SDK 10.0.19041.0 with 10.0.15063.0 as the minimum. To build a sideload package, create a test certificate under `Package.appxmanifest` > Packaging (the `.pfx` is git-ignored).

## Third-party

- [miniz](https://github.com/richgel999/miniz) (MIT)
