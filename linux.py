#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lafdup Linux 本地编译脚本。

在仓库根 build/ 目录用 CMake 编译桌面客户端，复制到 dist/linux-x64/。

用法:
  ./linux.py [--reconfigure]
"""

import argparse
import logging
import os
import shutil
import subprocess
import sys
from pathlib import Path

logging.basicConfig(
    level=logging.INFO,
    format="%(levelname)s: %(message)s",
    stream=sys.stderr,
)
logger = logging.getLogger(__name__)

PC_ROOT = Path(__file__).resolve().parent
LAFDUP_ROOT = PC_ROOT.parent
BUILD_DIR = LAFDUP_ROOT / "build"
DIST_DIR = LAFDUP_ROOT / "dist" / "linux-x64"
ARTIFACT = "lafdup"


def run(cmd, **kwargs):
    logger.debug("run: %s", " ".join(str(c) for c in cmd))
    return subprocess.run(cmd, check=True, **kwargs)


def configure(force: bool) -> None:
    BUILD_DIR.mkdir(parents=True, exist_ok=True)
    cache = BUILD_DIR / "CMakeCache.txt"
    if cache.is_file() and not force:
        return
    run(
        ["cmake", str(PC_ROOT), "-DLAFRPC_USE_QTNG=ON"],
        cwd=BUILD_DIR,
    )


def build() -> Path:
    run(["cmake", "--build", str(BUILD_DIR), "--target", ARTIFACT, "-j4"])
    exe = BUILD_DIR / ARTIFACT
    if not exe.is_file():
        logger.error("未找到编译产出: %s", exe)
        sys.exit(1)
    return exe


def install_artifact(src: Path) -> None:
    DIST_DIR.mkdir(parents=True, exist_ok=True)
    dest = DIST_DIR / ARTIFACT
    shutil.copy2(str(src), str(dest))
    os.chmod(str(dest), 0o755)
    logger.info("copy file: %s -> %s", src, dest)


def main() -> None:
    parser = argparse.ArgumentParser(description="lafdup Linux 本地编译")
    parser.add_argument(
        "--reconfigure",
        action="store_true",
        help="强制重新运行 cmake",
    )
    args = parser.parse_args()

    configure(args.reconfigure)
    install_artifact(build())
    logger.info("完成。Linux 可执行文件在 %s/", DIST_DIR)


if __name__ == "__main__":
    main()
