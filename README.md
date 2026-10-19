# JAV-Qt
## Supported Providers
DMM (FANZA) <br>
JavLib (English titles and genres, needs FlareSolverr)
## Installation
Download the file for your system from the [latest release](../../releases/latest).
### Windows (x86_64)
Extract `JAV-Qt-…-windows-x86_64.zip` and run `JAV-Qt.exe`. <br>
If Windows shows *"Windows protected your PC"*, click *More info*, then *Run anyway*.
### macOS (Apple silicon)
Open `JAV-Qt-…-macos-arm64.dmg` and drag JAV-Qt to Applications. <br>
If macOS cannot verify the app, click *Open Anyway* in *System Settings → Privacy & Security*, or run `xattr -dr com.apple.quarantine /Applications/JAV-Qt.app`.
### Linux (x86_64)
Run `chmod +x JAV-Qt-…-linux-x86_64.AppImage` and start it.
## Usage
Click *Import* and pick the folder with your videos. Their IDs are read from the file or folder names (e.g. `MKMP-705`). <br>
Rate videos with stars, bookmark them, and sort the library by ID, stars or bookmark. <br>
Zoom the library with *Ctrl + mouse wheel*, *Ctrl+=* and *Ctrl+-* (*Ctrl+0* resets). <br>
Switch between the *Default Dark* and *Yotsuba B* themes with *Select theme*.
## Prerequisites (optional)
Only needed for English titles and genres from JavLib; without them, titles are in Japanese.
### Docker
(Gentoo) Install with `sudo emerge app-containers/docker app-containers/docker-cli` <br>
(Debian/Ubuntu) Install with `sudo apt install docker.io` <br>
(Windows) Install with `winget install -e --id Docker.DockerDesktop` <br>
(macOS) Install [Docker Desktop](https://www.docker.com/products/docker-desktop/)
### FlareSolverr
Install with `docker run -d --name=flaresolverr -p 127.0.0.1:8191:8191 -e LOG_LEVEL=info --restart unless-stopped ghcr.io/flaresolverr/flaresolverr:latest`
## Data
The library, notes, settings and log (`jav-qt.log`) are stored in: <br>
(Linux) `~/.local/share/JAV-Qt` <br>
(macOS) `~/Library/Application Support/JAV-Qt` <br>
(Windows) `%APPDATA%\JAV-Qt`
## Building
Requires CMake, a C++20 compiler and Qt 6.10 or newer: `cmake -S . -B build && cmake --build build` <br>
Set `JAVQT_DATA_DIR` to keep a development build's data apart.
## Releasing
Set the version in `CMakeLists.txt`, commit, then `git tag vX.Y.Z && git push origin vX.Y.Z`. GitHub Actions builds the three packages and publishes the release.
