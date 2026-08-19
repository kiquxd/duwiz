# ya-ncdu

`ya-ncdu` is an interactive terminal file browser and disk-usage analyzer. It
calculates directory sizes concurrently and prepares file previews on a separate
worker so navigation remains responsive.

Features:

- directory browsing and parallel size calculation;
- UTF-8/UTF-16 text, bounded hex, and PNG/JPEG/GIF/BMP previews;
- lexical highlighting for C/C++, Python, Bash, JSON, CMake, and Markdown;
- a fullscreen preview that requests an image at the larger viewport size;
- PDF detection and opening in the native system viewer;
- create, rename, and delete files or empty directories;
- cancellation and stale-result rejection for background work.

> [!WARNING]
> Confirmed deletion operates on the real filesystem and does not use Trash.
> Non-empty directories are deliberately not deleted.

## Supported platforms

Only Linux x86-64 and macOS 13+ arm64 (Apple Silicon) are supported. Windows,
Linux arm64, and Intel macOS are not supported. The Docker image is Linux
x86-64 and can run under Docker Desktop emulation on Apple Silicon.

## Dependencies

The application uses C++20, CMake 3.24+, pinned FTXUI, and `preview_lib` as a
Git submodule. `preview_lib` vendors stb for image decoding; it no longer uses
PDFium or system image libraries. On Linux, `xdg-open` from `xdg-utils` is used
for PDF opening. macOS uses the built-in `open` command.

Install prerequisites:

```sh
./install_deps.sh
```

On Linux this supports apt, dnf, pacman, zypper, and apk. On macOS, install
[Homebrew](https://brew.sh/) and Apple Command Line Tools first:

```sh
xcode-select --install
./install_deps.sh
```

A full Xcode installation is not required.

## Build from source

```sh
git clone --recurse-submodules https://github.com/kiquxd/ya-ncdu.git
cd ya-ncdu
./scripts/bootstrap_dependencies.sh
make run
./build/ya-ncdu --path "$HOME"
```

For an existing clone:

```sh
git pull
git submodule update --init --recursive
./scripts/bootstrap_dependencies.sh
make run
```

`make run` keeps its historical name: it configures and builds, but does not
launch the fullscreen application. Run `make test` when you also want the test
suite. Bootstrap downloads only the pinned
FTXUI checkout; the preview library's stb headers are already vendored.
`scripts/configure.sh` automatically refreshes legacy PDFium-based or relocated
CMake caches.

## Install the binary

No prebuilt release is currently published. After building:

```sh
cmake --install build --prefix "$HOME/.local"
export PATH="$HOME/.local/bin:$PATH"
ya-ncdu --path "$HOME"
```

This installs the executable, `libpreview.so`/`libpreview.dylib`, the one public
preview header, CMake package metadata, and license notices. The executable uses
a relative runtime search path, so `LD_LIBRARY_PATH`/`DYLD_LIBRARY_PATH` should
not be needed. For a system-wide install:

```sh
sudo cmake --install build --prefix /usr/local
```

## Controls

| Key | Action |
|---|---|
| Up / Down | Select an entry and request its preview |
| Enter / Right | Enter the selected directory |
| Left | Return to the previous directory |
| `p` | Open/close fullscreen preview (`Esc` also closes it) |
| `o` | Open the selected PDF in the system viewer |
| `c` | Create a file |
| `m` | Create a directory |
| `r` | Rename the selected entry |
| `d` | Delete a file or empty directory after confirmation |
| `q` | Quit |

The right pane is intentionally compact. An image can look soft there because
many source pixels must be reduced to few terminal cells. Fullscreen mode does
not enlarge that small result: it asks `preview_lib` to decode/resize again for
the full terminal viewport. Rendering uses ANSI true color and the `▀` half
block, giving two vertical pixels per cell. WebP is intentionally unsupported.

PDFs are not rendered inside the terminal. The preview pane identifies them and
`o` starts `open` on macOS or `xdg-open` on Linux. The viewer is never launched
automatically.

## Docker

```sh
git submodule update --init --recursive
docker build --platform=linux/amd64 -t ya-ncdu .
docker run --rm -it --platform=linux/amd64 \
  --user "$(id -u):$(id -g)" \
  --mount type=bind,source="$HOME",target=/data \
  ya-ncdu --path /data
```

The bind mount is writable by default, so mutations affect the host. Add
`,readonly` for safe browsing. A graphical system PDF viewer is generally not
available inside the container; `o` may therefore report that `xdg-open` could
not be started.

## Development and tests

```sh
make run
make test
```

To work against a separate library checkout without moving the submodule pin:

```sh
PREVIEW_ROOT=/path/to/preview_lib ./scripts/bootstrap_dependencies.sh
PREVIEW_ROOT=/path/to/preview_lib make run
```

`preview_lib` is synchronous and UI-independent. `ya-ncdu` owns its worker,
debounce, cancellation, pane sizing, semantic syntax theme, and external viewer
action.
