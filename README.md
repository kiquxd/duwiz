# duwiz

`duwiz` (du wizard) is an interactive terminal file browser and disk-usage
analyzer. Directory sizes and file previews are prepared in the background so
the interface remains responsive.

## Features

- browse directories and calculate disk usage;
- preview text, source code, Markdown, JSON, CSV/TSV, ZIP/TAR archives and
  binary files;
- preview PNG, JPEG, GIF and BMP images using Kitty Unicode placeholders with
  an automatic ANSI fallback;
- open PDF files in the system viewer;
- open a preview fullscreen without reusing a low-resolution sidebar image;
- create, rename and delete files or empty directories;
- cancel obsolete background preview and size-calculation work while browsing.

> [!WARNING]
> Deletion is permanent and does not use Trash. `duwiz` refuses to delete
> non-empty directories.

## Install

Supported hosts are Linux x86-64 and macOS 13+ on Apple Silicon. Windows,
Linux arm64 and Intel macOS are not supported.

Clone the repository, install the build prerequisites, then run the installer:

```sh
git clone https://github.com/kiquxd/duwiz.git
cd duwiz
./install_deps.sh
./install.sh
```

On macOS, install [Homebrew](https://brew.sh/) and Apple Command Line Tools
before running `install_deps.sh`:

```sh
xcode-select --install
```

`install_deps.sh` supports apt, dnf, pacman, zypper and apk on Linux. You may
skip it when a C++20 compiler, CMake 3.24+ and Git are already installed. Linux
also needs `xdg-open` from `xdg-utils` to open PDFs.

`install.sh` initializes the `preview_lib` submodule, downloads the pinned
FTXUI source, builds the application and installs it under `$HOME/.local`.
The executable is placed at `$HOME/.local/bin/duwiz`; its required preview
library is installed alongside it and needs no `LD_LIBRARY_PATH` or
`DYLD_LIBRARY_PATH` configuration.

If `$HOME/.local/bin` is not in `PATH`, add it to your shell profile and start a
new shell:

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Then run:

```sh
duwiz
```

To install under another writable prefix:

```sh
./install.sh --prefix /another/prefix
```

### Update

From the existing clone:

```sh
git pull
./install.sh
```

The installer also moves the submodule to the revision pinned by the updated
`duwiz` checkout.

## Usage

Without arguments, `duwiz` starts in `$HOME` and uses one filesystem scanning
worker. The preview worker remains separate.

```sh
duwiz                  # open $HOME with one scanning worker
duwiz -p /some/path    # start in another directory
duwiz -j 4             # use four scanning workers
duwiz --help
```

Options:

| Option | Meaning |
|---|---|
| `-p PATH`, `--path PATH` | Start directory; defaults to `$HOME` |
| `-j THREADS` | Number of filesystem scanning workers; defaults to `1` |
| `-h`, `--help` | Show command-line help |

## Controls

| Key | Action |
|---|---|
| Up / Down | Select an entry |
| Enter / Right | Enter the selected directory |
| Left | Return to the previous directory |
| `p` | Open or close fullscreen preview |
| `o` | Open the selected PDF in the system viewer |
| `c` | Create a file |
| `m` | Create a directory |
| `r` | Rename the selected entry |
| `d` | Ask to delete a file or empty directory |
| `y` / `n` | Confirm or cancel deletion |
| Esc | Close fullscreen preview or cancel text input |
| `q` | Quit |

## Preview behavior

| Content | Behavior |
|---|---|
| Text | UTF-8, UTF-8 BOM and UTF-16LE/BE BOM, line numbers, wrapping and bounded hex fallback for binary data |
| Source | Dependency-free highlighting for C/C++, Python, Bash, JSON and CMake |
| Markdown | Headings, emphasis, links, lists, tasks, quotes, fenced code and viewport-aware tables |
| JSON | Validated and indented preview |
| CSV/TSV | Bounded tables with quoted-field parsing and numeric alignment |
| ZIP/TAR | Bounded archive listings without extraction |
| Images | PNG, JPEG, first-frame GIF and BMP; JPEG orientation is applied |
| PDF | Detection only; press `o` to open the system viewer |

Markdown support is a practical terminal-oriented subset, not a complete
CommonMark/GFM implementation. WebP, ZIP64 and extended TAR names are currently
unsupported.

Kitty terminals use Unicode graphics placeholders and a physical-pixel image.
Other terminals use ANSI true color and the `▀` half block. Automatic mode uses
ANSI inside tmux because Kitty command passthrough is not implemented. Override
detection for diagnostics with `DUWIZ_IMAGE_BACKEND=kitty` or
`DUWIZ_IMAGE_BACKEND=ansi`.

## Docker

Initialize the submodule before building because the Docker context does not
contain Git metadata:

```sh
git submodule update --init --recursive
docker build --platform=linux/amd64 -t duwiz .
docker run --rm -it --platform=linux/amd64 \
  --user "$(id -u):$(id -g)" \
  --mount type=bind,source="$HOME",target=/data \
  duwiz -p /data
```

The bind mount is writable, so create, rename and delete operations affect the
host. Add `,readonly` to the mount for read-only browsing. The runtime image
does not include a graphical PDF viewer. On Apple Silicon, Docker Desktop runs
this Linux x86-64 image under emulation.

## Development and tests

The installer is the normal user-facing path. For development, prepare the
pinned sources and use the Make targets without installing:

```sh
./scripts/bootstrap_dependencies.sh
make
make test
./build/duwiz
```

To develop against a separate `preview_lib` checkout without changing the
submodule revision:

```sh
PREVIEW_ROOT=/path/to/preview_lib ./scripts/bootstrap_dependencies.sh
PREVIEW_ROOT=/path/to/preview_lib make
```

`preview_lib` is synchronous and UI-independent. `duwiz` owns background work,
cancellation, pane sizing, terminal rendering and the external PDF viewer
action.
