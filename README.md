# JAV-Qt

JAV-Qt organizes the JAV videos on your computer. Point it at the folder with
your videos and it finds each video's ID in its file or folder name (e.g.
`MKMP-705`), downloads its details (title, cast, release date, genres, …), its
cover and sample pictures, and shows your collection as a searchable library.

Details come from [DMM/FANZA](https://www.dmm.co.jp) first, with English titles
and genres from [JavLibrary](https://www.javlibrary.com) (see
[English titles](#optional-english-titles-from-javlibrary)).

## Download and install

Download the file for your system from the **[latest release](../../releases/latest)**:

| System | File |
|---|---|
| Windows 10/11 (64-bit) | `JAV-Qt-…-windows-x86_64.zip` |
| macOS 13 or newer, Apple silicon (M1 and later) | `JAV-Qt-…-macos-arm64.dmg` |
| Linux (64-bit) | `JAV-Qt-…-linux-x86_64.AppImage` |

### Windows

1. Right-click the downloaded `.zip` and choose **Extract All…**. Put the
   `JAV-Qt` folder wherever you like, e.g. in `Documents`.
2. Open the folder and double-click **JAV-Qt.exe**.
3. The first time, Windows may show *"Windows protected your PC"*: click
   **More info**, then **Run anyway**. (This appears for apps from small
   developers who have not bought a code signing certificate.)

Tip: right-click `JAV-Qt.exe` and choose **Send to → Desktop (create shortcut)**.

### macOS

1. Open the downloaded `.dmg` and drag **JAV-Qt** onto the **Applications** folder.
2. Open JAV-Qt from Applications. The first time, macOS says it *cannot verify*
   the app, because it is not registered with Apple. Click **Done** (or
   **Cancel**), then open **System Settings → Privacy & Security**, scroll down
   and click **Open Anyway** next to the message about JAV-Qt, and confirm.
   You only need to do this once.

   Alternatively, run this in Terminal once:

   ```sh
   xattr -dr com.apple.quarantine /Applications/JAV-Qt.app
   ```

### Linux

1. Make the downloaded file executable: right-click it, **Properties →
   Permissions → Allow executing file as program**, or in a terminal:

   ```sh
   chmod +x JAV-Qt-*-linux-x86_64.AppImage
   ```

2. Double-click it, or run `./JAV-Qt-*-linux-x86_64.AppImage`.

If it does not start and mentions FUSE, install FUSE 2: on Ubuntu 24.04 and
newer `sudo apt install libfuse2t64`, on older Ubuntu and Debian
`sudo apt install libfuse2`, on Fedora `sudo dnf install fuse-libs`.

## Using JAV-Qt

1. Click **Import From Folder (Library)** at the top right and choose the folder
   with your videos. Subfolders are included.
2. JAV-Qt finds the ID in each video's file name, like `hhd800.com@MKMP-705.mp4`,
   or else in its folder name, like `MKMP-705/video.mp4`, and starts downloading
   the details. Videos appear in the library as they are done; the top bar shows
   how many are left.
3. Search by ID or title, or filter by actress, director, maker and genres on
   the left. Double-click a video to see its details, play it with your usual
   video player, and write your own notes. Click a name or genre there to see
   all videos with it.

Choosing **Import From Folder** again with another folder switches to it. Videos
already downloaded stay in the library.

## Optional: English titles from JavLibrary

DMM, the main source, has Japanese titles and genres only. For English titles
and genres, JAV-Qt also asks JavLibrary, which it can only reach through
[FlareSolverr](https://github.com/FlareSolverr/FlareSolverr), a small helper
that runs in Docker. Without it everything else works, but titles and genres
are in Japanese, and videos that DMM does not have are not found.

1. Install Docker:
   - **Windows and macOS:** install [Docker Desktop](https://www.docker.com/products/docker-desktop/)
     (Windows: `winget install -e --id Docker.DockerDesktop`) and start it.
   - **Linux:** install Docker from your distribution, e.g.
     `sudo apt install docker.io` (Debian/Ubuntu),
     `sudo emerge app-containers/docker app-containers/docker-cli` (Gentoo),
     and start it with `sudo systemctl enable --now docker`.
2. Start FlareSolverr once; Docker restarts it automatically from then on:

   ```sh
   docker run -d --name=flaresolverr -p 127.0.0.1:8191:8191 -e LOG_LEVEL=info --restart unless-stopped ghcr.io/flaresolverr/flaresolverr:latest
   ```

   On Linux, put `sudo` in front if Docker says *permission denied*.

3. Restart JAV-Qt. Videos found while FlareSolverr was not running are not
   updated automatically; they keep their Japanese details.

JavLibrary is asked at most once every 15 seconds, so a large library takes a
while the first time.

## Your data

JAV-Qt keeps its database (including your notes), the downloaded pictures,
its settings and a log file `jav-qt.log` in:

| System | Folder |
|---|---|
| Windows | `%APPDATA%\JAV-Qt` (paste this into the Explorer address bar) |
| macOS | `~/Library/Application Support/JAV-Qt` (Finder: **Go → Go to Folder…**) |
| Linux | `~/.local/share/JAV-Qt` |

Your videos are never moved or changed. To uninstall, delete the app (the
folder, the app in Applications, or the AppImage) and, to also remove your
library, the folder above.

## Building from source

You need CMake 3.21 or newer, a C++20 compiler and Qt 6.10 or newer
(Widgets, Network, Concurrent). spdlog and gumbo are used from your system when
installed, else downloaded and built automatically.

```sh
cmake -S . -B build
cmake --build build
./build/jav-qt
```

`-DJAVQT_BUNDLED_DEPS=ON` always builds spdlog and gumbo from source, as the
releases do. To keep the data of a development build apart from your real
library, set `JAVQT_DATA_DIR`, e.g. `JAVQT_DATA_DIR=$PWD/dev-data ./build/jav-qt`.

## Making a release

Releases are built by GitHub Actions (`.github/workflows/build.yml`) on GitHub's
own Linux, Windows and macOS machines; every push to `main` builds all three
packages, which can be downloaded from the run's page under **Actions**. To
publish a release:

1. Set the new version in `CMakeLists.txt` (`project(JAV-Qt VERSION x.y.z …)`)
   and commit it.
2. Tag the commit with that version and push the tag:

   ```sh
   git tag v0.1.0
   git push origin v0.1.0
   ```

3. After about 15 minutes the release appears under **Releases**, with the
   three packages attached and notes listing the changes since the last one.

## License

The bundled fonts are under the SIL Open Font License: Inter
(`assets/fonts/inter/OFL.txt`) and Noto Sans CJK JP
(`assets/fonts/noto-sans-cjk-jp/LICENSE`). JAV-Qt uses Qt under the LGPL v3,
spdlog (MIT) and gumbo (Apache 2.0).
