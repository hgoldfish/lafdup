SyncClipboard
=============

![Screenshot Of SyncClipboard](https://raw.githubusercontent.com/hgoldfish/lafdup/master/images/screenshot.png)


Introduction
------------

Synchronize clipboard between computers by network broadcast. Data is encrypted using aes256-cbf.


Download
--------

[Windows Version](https://qtng.org/lafdup.7z)

[Android Version](https://play.google.com/store/apps/details?id=com.hgoldfish.lafdup)


License
-------

SyncClipboard is distributed under GPL 3.0 license.


Building
--------

Require Qt 5.x to build.

Using cmake to build this project.

    $ git clone https://github.com/hgoldfish/lafdup.git
    $ cd lafdup
    $ git clone https://github.com/hgoldfish/lafrpc.git
    $ git clone https://github.com/hgoldfish/qtng.git
    $ mkdir build
    $ cd build
    $ cmake .. -DLAFRPC_USE_QTNG=ON
    $ make -j4

The network library comes from [qtng](https://github.com/hgoldfish/qtng)'s Qt
binding (`qtng/qt`, compatible `qtnetworkng` target); pass
`-DLAFRPC_USE_QTNG=ON` to use it. The deprecated standalone
[qtnetworkng](https://github.com/hgoldfish/qtnetworkng) project is still
supported as a fallback when qtng is not available.

### Windows XP binaries (Docker / mingw-w64)

Cross-compile the desktop client for Windows XP with mingw-w64 (MSVCRT, i686)
inside a Debian bookworm container. Requires Docker and sibling checkouts of
`lafrpc` and `qtng`.

From the `pc` directory:

    $ ./winxp.py
    # optional: ./winxp.py --no-cache

Artifacts are copied to `dist/win32/lafdup.exe`. The build automatically
uses the qtng Qt binding when the qtng checkout is present, and falls back to
the standalone qtnetworkng project otherwise.
