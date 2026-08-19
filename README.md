# ya-ncdu

`ya-ncdu` is an interactive terminal file browser and disk-usage analyzer. It
calculates directory sizes in the background and shows an asynchronous preview
of the selected file without blocking navigation.

## Features

- browse directories and calculate entry sizes concurrently;
- preview UTF-8/UTF-16 text and bounded hexadecimal dumps;
- preview PNG, JPEG, first-frame GIF, BMP, and PDF pages in the terminal;
- create files and directories, rename entries, and delete files or empty
  directories;
- cancel obsolete preview and directory-scan work when navigation moves on.

> [!WARNING]
> Delete operates on the real filesystem and does not use Trash. The program
> asks for confirmation, but a confirmed deletion cannot be undone by
> `ya-ncdu`.

## Supported platforms

Only these native targets are supported:

| Platform | Architecture | Status |
|---|---:|---|
| Linux | x86-64 | Supported and tested |
| macOS 13+ | arm64 / Apple Silicon | Supported; requires full Xcode |

Windows, Linux arm64, and Intel macOS are not supported. A Docker image is
always Linux x86-64; Docker Desktop can run it under emulation on Apple
Silicon.

## Dependencies

The common requirements are Git, CMake 3.24 or newer, Python 3, curl, an
XZ-capable tar, and enough free disk space for a local PDFium build. The first
bootstrap is large and may take a while; generated dependencies live in
ignored `.deps` directories and are reused on later builds.

The repository pins:

- [`preview_lib`](https://github.com/kiquxd/ai-driven_preview-lib) as a Git
  submodule;
- FTXUI 7.0.1 at a fixed commit;
- a fixed PDFium revision and its transitive build dependencies.

No system FTXUI, PDFium, Chafa, or libmagic installation is used.

### Linux x86-64

`install_deps.sh` supports apt, dnf, pacman, zypper, and apk. It installs the
C/C++ toolchain, CMake, Git, curl, XZ support, CA certificates, and Fontconfig:

```sh
./install_deps.sh
```

Your distribution must provide CMake 3.24 or newer. If its package is older,
install a newer CMake before configuring the project.

### macOS arm64

A complete Xcode installation is required. The standalone Command Line Tools
package is not enough because PDFium queries `xcodebuild` and the Xcode SDK.
After installing Xcode from Apple, select and initialize it:

```sh
sudo xcode-select --switch /Applications/Xcode.app/Contents/Developer
sudo xcodebuild -runFirstLaunch
xcodebuild -version
xcrun --sdk macosx --show-sdk-path
```

Install [Homebrew](https://brew.sh/) if necessary, then install the remaining
tools:

```sh
./install_deps.sh
```

## Build from source

Clone the submodule together with the application:

```sh
git clone --recurse-submodules https://github.com/kiquxd/ya-ncdu.git
cd ya-ncdu
```

For an existing checkout:

```sh
git pull
git submodule update --init --recursive
```

Prepare pinned dependencies and build:

```sh
./install_deps.sh
./scripts/bootstrap_dependencies.sh
make run
```

Despite its historical name, `make run` configures and builds; it does not
launch the fullscreen TUI. Run it separately:

```sh
./build/ya-ncdu --path "$HOME"
```

The bootstrap is resumable. Do not delete `.deps` after an interrupted build;
run the same command again. `scripts/configure.sh` also refreshes a CMake cache
that was copied or moved from another machine.

## Install the binary

There are currently no prebuilt release binaries. After building from source,
install the executable, `preview_lib` shared library, CMake package files, and
third-party notices under one prefix:

```sh
cmake --install build --prefix "$HOME/.local"
```

Make sure the selected prefix's binary directory is on `PATH`:

```sh
export PATH="$HOME/.local/bin:$PATH"
ya-ncdu --path "$HOME"
```

The installed executable uses a relative runtime search path for its bundled
`libpreview.so` or `libpreview.dylib`; `LD_LIBRARY_PATH` and
`DYLD_LIBRARY_PATH` should not be necessary. For a system-wide installation:

```sh
sudo cmake --install build --prefix /usr/local
```

## Usage

```text
ya-ncdu [options]

  -h, --help          Show command-line help
  -p, --path PATH     Start in PATH (default: current directory)
  -j THREADS          Maximum parallel directory-size jobs (default: 4)
```

Main controls:

| Key | Action |
|---|---|
| Up / Down | Select an entry and request its preview |
| Enter / Right | Enter the selected directory |
| Left | Return to the previous directory |
| `c` | Create a file |
| `m` | Create a directory |
| `r` | Rename the selected entry |
| `d` | Delete a file or empty directory after confirmation |
| `q` | Quit |

The preview pane appears on the right. Image and PDF previews use terminal
true-color cells; results depend on the terminal's color support and available
viewport size. WebP is intentionally unsupported. PDF password entry and page
navigation are not exposed by the current TUI.

## Docker

Initialize the submodule before creating the build context:

```sh
git submodule update --init --recursive
docker build --platform=linux/amd64 -t ya-ncdu .
```

Run against a host directory by mounting it into `/data`:

```sh
docker run --rm -it --platform=linux/amd64 \
  --user "$(id -u):$(id -g)" \
  --mount type=bind,source="$HOME",target=/data \
  ya-ncdu --path /data
```

The mount is writable by default, so create/rename/delete operations affect the
host directory. The explicit user mapping avoids creating root-owned files on
the host. Add `,readonly` to the mount for safe browsing. The Docker build
compiles PDFium and can therefore be slow on the first run, especially when
x86-64 is emulated on Apple Silicon. Because the container performs a Linux
build, full Xcode is not required for this Docker-only workflow.

## Development and tests

```sh
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To work on a separate `preview_lib` checkout without changing the pinned
submodule revision:

```sh
PREVIEW_ROOT=/path/to/preview_lib ./scripts/bootstrap_dependencies.sh
PREVIEW_ROOT=/path/to/preview_lib make run
```

The preview backend is synchronous and UI-agnostic; `ya-ncdu` owns the worker,
cancellation, stale-result rejection, and FTXUI rendering. PDFium runs
in-process, so untrusted PDFs are not isolated in a sandbox.
