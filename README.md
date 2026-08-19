# Yet Another NCDU

## About

This is a tool that is capable of being file browser with support of file manipulating such as creating new files/directories, renaming and deleting files together with disk analyzer tool

`TODO`: default to only browsing mode with enabling disk analyzer option.

## Installation

### Script

Run `install.sh` and it will set everything up

### Compiling from source

If you want to compile from source, then you'll need:

#### Dependencies

- CMake 3.24+
- a C++20 compiler
- `preview_lib`, pinned at `third_party/preview_lib` as a Git submodule

#### Command

```sh
./scripts/bootstrap_dependencies.sh
make run
./build/ya-ncdu --path third_party/preview_lib/tests/corpus
```

The preview pane is backed by `preview_lib` and supports text, bounded hex,
PNG/JPEG/GIF/BMP, and PDF. Preview work runs on a dedicated cancellable worker,
so moving through the directory does not decode files on the FTXUI event loop.
FTXUI v7.0.1 is fetched at a pinned commit into the ignored `.deps` directory.
The same bootstrap initializes `third_party/preview_lib` as a Git submodule and
prepares PDFium for the current supported platform. CMake builds and links
`preview::preview` directly; there is no hard-coded `.so`/`.dylib` filename.

Clone this repository together with its submodules:

```sh
git clone --recurse-submodules https://github.com/kiquxd/ya-ncdu.git
```

For an existing checkout, run `git submodule update --init --recursive`. The
bootstrap command also performs this initialization when the submodule is
configured but has not been checked out yet. `PREVIEW_ROOT=/path/to/preview_lib`
remains available for local library development.

Supported targets are Linux x86-64 and macOS arm64. On macOS, install full
Xcode; the standalone Command Line Tools package is not sufficient to build
PDFium. If a checkout was moved from another machine,
`scripts/configure.sh` detects and refreshes the stale absolute CMake cache.
