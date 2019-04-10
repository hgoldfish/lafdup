SyncClipboard
=============


Introduction
------------

Synchronize clipboard between computers by network broadcast. Data is encrypted using aes256-cbf.


Download
--------

[Windows Version](https://qtng.org/sync_clipboard.7z)


License
-------

SyncClipboard is distributed under GPL 3.0 license.


Building
--------

Require Qt 5.x to build.

Using cmake to build this project.

    $ git clone https://github.com/hgoldfish/lafdup.git
    $ cd lafdup
    $ git clone https://github.com/hgoldfish/qtnetworkng.git
    $ mkdir build
    $ cd build
    $ cmake ..
    $ make -j4
