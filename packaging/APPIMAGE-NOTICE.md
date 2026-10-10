# Trait64 AppImage

Trait64 is AGPL-3.0-or-later. The AppImage bundles dynamically linked Qt 6 libraries,
QML modules and runtime dependencies from Debian 12. Qt copyright/license notices
and exact source package versions accompany the AppImage in
`usr/share/doc/trait64/runtime-licenses` and `runtime-sources.tsv`.

The accompanying `Trait64-runtime-sources.tar` release asset provides the Debian
source archives, including Debian patches, for bundled runtime packages. Trait64
sources for the exact release tag are available from the same GitHub Release.
No Stockfish binary is bundled: install your distribution's `stockfish` package
or select your own UCI executable. Stockfish then runs entirely offline.

Qt remains dynamically replaceable. Run `./Trait64.AppImage --appimage-extract`,
replace the relevant libraries/plugins under `squashfs-root/usr`, and run
`squashfs-root/AppRun`. You may modify these libraries and reverse engineer the
combined work to debug your modifications, as permitted by their licenses.
The AppImage requires Linux x86_64 and glibc >= 2.36 (Debian 12 baseline).
Use `APPIMAGE_EXTRACT_AND_RUN=1 ./Trait64.AppImage` when FUSE is unavailable.

Build tools: linuxdeploy and linuxdeploy-plugin-qt (MIT), pinned asset IDs and
SHA-256 hashes in `packaging/appimage.sh`; they are not incorporated into Trait64's
application source. The AppImage runtime and bundling tool notices accompany the
build-tool distributions and their upstream projects.
