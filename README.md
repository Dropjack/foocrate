# FooCrate

FooCrate is a native, integrated user interface component for foobar2000 x64 and Columns UI.

It recreates the familiar structure and everyday workflow of the Foobox Basic layout using C++, Win32, Direct2D, and DirectWrite. FooCrate is not an official Foobox release or code fork, and it does not require JSplitter, Spider Monkey Panel, JScript Panel, or another JavaScript UI host.

## Features

- Native playback controls, seek bar, volume, mute, output-device selection, and seven playback-order modes.
- Virtualized Playlist View with grouping, album artwork, configurable columns, sorting, queue operations, drag and drop, ratings, and standard context menus.
- Playlist Browser and album-only Cover Browser.
- Now Playing artwork, track metadata, playback statistics, and optional embedded ESLyric display.
- Light and dark themes, four built-in colour presets, Windows colour following, and album-artwork-derived themes.
- Playlist and track location commands, including restoration of the last viewed or played item.
- Three startup behaviours:
  - resume the last track and playback position;
  - restore the last track without playing;
  - start stopped at the Default Playlist home position.
- DPI-aware rendering, high-contrast fallback, keyboard handling, versioned settings migration, and bounded artwork/theme caches.

## Requirements

FooCrate 1.0.5 was validated with:

- Windows 10 or Windows 11 x64;
- foobar2000 2.25.10 stable x64;
- Columns UI 3.5.0;
- ESLyric 1.0.6.7 for optional lyrics;
- Playback Statistics 3.1.10 for optional five-star ratings and history-based autoplaylist presets.

Columns UI is required because it hosts the FooCrate root panel. ESLyric and Playback Statistics are optional.

FooCrate does not bundle, download, replace, or automatically update these third-party components. If ESLyric is unavailable, the lyrics area safely falls back to metadata. If Playback Statistics is unavailable, playback and browsing continue to work while rating actions and history-based autoplaylist presets report that the feature is unavailable.

## Installation

1. Install Columns UI and restart foobar2000.
2. Install any optional components you want to use, such as ESLyric and Playback Statistics.
3. Open `File > Preferences > Components`.
4. Select `Install...` and choose `FooCrate-1.0.5.fb2k-component`.
5. Restart foobar2000 and make sure the active user interface module is Columns UI.
6. Import the compatible `FooCrate-1.0.0.fcl`, or add the `FooCrate` root panel manually in the Columns UI layout editor.

The optional FCL contains a neutral default layout. It does not overwrite ESLyric's private settings, such as fonts, lyric sources, storage paths, or custom display rules.

Release files are provided separately:

- `FooCrate-1.0.5.fb2k-component` — installable component;
- `FooCrate-1.0.5-SHA256SUMS.txt` — integrity checksum;
- `FooCrate-1.0.5-RELEASE-NOTES.md` — release-specific changes;
- `FooCrate-1.0.0.fcl` — optional compatible Columns UI layout.

## Upgrading

Install the newer `.fb2k-component` through the foobar2000 Components page and restart when prompted.

Stable component, panel, command, and settings identifiers are preserved across the 1.0.x upgrades. Existing FooCrate layouts, themes, grouping rules, columns, and shortcut references should remain recognizable. The optional FCL is never imported automatically and does not replace an existing Columns UI layout.

## Building from source

The project requires Visual Studio 2022 with the MSVC x64 toolchain and CMake support.

```powershell
$CMake = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$CTest = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe'

& $CMake --preset vs2022-x64
& $CMake --build --preset debug
& $CTest --preset debug
& $CMake --build --preset release
& $CTest --preset release
& $CMake --build --preset package-release
```

The packaged component is written to the repository `dist` directory. foobar2000 and third-party component binaries are not part of the FooCrate package.

## Scope of version 1.0

FooCrate 1.0 does not yet include:

- a complete screen-reader accessibility object tree;
- final production icon artwork;
- a native Mini Player;
- a standalone CoverFlow window.

These limitations do not affect the main integrated playback, playlist, album-browsing, theme, or settings workflows.

## Project documentation

The detailed product specification, implementation tasks, development boundaries, and verification records are maintained in Chinese:

- [`specs/PRODUCT_SPEC.md`](specs/PRODUCT_SPEC.md)
- [`docs/DEVELOPMENT_SETUP.md`](docs/DEVELOPMENT_SETUP.md)
- [`docs/INSTALLATION_AND_UPGRADE.md`](docs/INSTALLATION_AND_UPGRADE.md)
- [`docs/RELEASE_NOTES_1.0.5.md`](docs/RELEASE_NOTES_1.0.5.md)
- [`docs/RELEASE_NOTES_1.0.4.md`](docs/RELEASE_NOTES_1.0.4.md)
- [`docs/RELEASE_NOTES_1.0.0.md`](docs/RELEASE_NOTES_1.0.0.md)
- [`tasks/README.md`](tasks/README.md)

## Acknowledgements

FooCrate is visually and ergonomically inspired by Foobox Basic. The name “Crate” preserves that design lineage while distinguishing this native implementation from Foobox itself.

foobar2000, Columns UI, ESLyric, and Playback Statistics are independent projects maintained and distributed by their respective authors.
