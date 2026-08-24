#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lafdup Windows XP mingw-w64 交叉编译脚本。

在 Debian bookworm 容器中用 mingw-w64（MSVCRT, i686）交叉编译 lafdup 桌面客户端，
依赖的 Qt 5.6.3（最后一个官方支持 Windows XP 的版本）在 Dockerfile.winxp 中
自行下载并交叉编译（静态、release、XP 兼容）。

构建时挂载本目录（pc）/ lafrpc / qtng 源码树；若检测到 qtng 仓库（qtng/qt 的
Qt binding），lafrpc 会自动切换为使用 qtng（LAFRPC_USE_QTNG=ON），否则回退到
已弃用的独立 qtnetworkng 项目。

用法:
  ./winxp.py [--no-cache]
      构建 Docker 镜像并编译 lafdup.exe，
      复制到仓库根 dist/win32/

环境变量:
  QT_SOURCE_MIRROR   自定义 Qt 5.6.3 源码包镜像 URL（内网镜像/自建代理），
                     设置后优先下载，失败才回落到 GitHub 与官方源。
"""

import argparse
import hashlib
import logging
import os
import shutil
import subprocess
import sys
import tarfile
from pathlib import Path
from typing import List, Tuple

logging.basicConfig(
    level=logging.INFO,
    format="%(levelname)s: %(message)s",
    stream=sys.stderr,
)
logger = logging.getLogger(__name__)

IMAGE_NAME = "lafdup-winxp-mingw"
DOCKERFILE_HASH_LABEL = "lafdup.dockerfile_hash"
# 构建缓存收在 pc/.build-winxp/；产物复制到仓库根 dist/。
# lafrpc / qtng 是 lafdup 的平级检出（LAFDUP_ROOT 的上级目录）。
PC_ROOT = Path(__file__).resolve().parent
LAFDUP_ROOT = PC_ROOT.parent
PROJECTS_ROOT = LAFDUP_ROOT.parent
DOCKERFILE_PATH = PC_ROOT / "Dockerfile.winxp"
QT_SOURCE_TAR = PC_ROOT / ".build-winxp" / "qtbase-5.6.3.tar.gz"
# 可选自定义 Qt 源码镜像（如内网缓存或自建代理），优先于内置源下载。
QT_SOURCE_MIRROR = os.environ.get("QT_SOURCE_MIRROR", "")
# 完整性阈值：GitHub 归档约 25MB、官方 xz 约 28MB，远大于此值。
QT_SOURCE_MIN_SIZE = 10_000_000
DOCKER_BUILD_ROOT = PC_ROOT / ".build-winxp" / "build"
DIST_DIR = LAFDUP_ROOT / "dist"
ARTIFACTS = ("lafdup.exe",)
ARCHES: Tuple[str, ...] = ("win32",)


def run(cmd: List[str], check: bool = True, quiet: bool = False) -> subprocess.CompletedProcess:
    logger.debug("run: %s", " ".join(cmd))
    kwargs = {"check": check, "universal_newlines": True}
    if quiet:
        kwargs["stdout"] = subprocess.DEVNULL
        kwargs["stderr"] = subprocess.DEVNULL
    return subprocess.run(cmd, **kwargs)


def check_docker() -> None:
    if run(["docker", "--version"], check=False, quiet=True).returncode != 0:
        logger.error("未检测到 Docker，请先安装并启动 Docker")
        sys.exit(1)
    if run(["docker", "info"], check=False, quiet=True).returncode != 0:
        logger.error("Docker 服务未运行")
        sys.exit(1)


def image_exists() -> bool:
    return run(["docker", "image", "inspect", IMAGE_NAME], check=False, quiet=True).returncode == 0


def compute_dockerfile_hash() -> str:
    digest = hashlib.sha256()
    if not DOCKERFILE_PATH.is_file():
        logger.error("找不到构建文件: %s", DOCKERFILE_PATH)
        sys.exit(1)
    # 流式读，避免把 25MB 源码包整体载入内存。
    for path in (DOCKERFILE_PATH, QT_SOURCE_TAR):
        if not path.is_file():
            continue
        with path.open("rb") as f:
            for chunk in iter(lambda: f.read(1024 * 1024), b""):
                digest.update(chunk)
    return digest.hexdigest()


def image_dockerfile_hash() -> str:
    if not image_exists():
        return ""
    result = subprocess.run(
        [
            "docker",
            "image",
            "inspect",
            "--format",
            '{{index .Config.Labels "%s"}}' % DOCKERFILE_HASH_LABEL,
            IMAGE_NAME,
        ],
        check=False,
        universal_newlines=True,
        stdout=subprocess.PIPE,
    )
    if result.returncode != 0:
        return ""
    value = (result.stdout or "").strip()
    if not value or value == "<no value>":
        return ""
    return value


def is_qt_tarball(path: Path) -> bool:
    """Return True if the file is a readable tar archive (gzip/xz, auto-detected).

    getmembers() walks every header to the end-of-archive marker and close()
    verifies the gzip/xz trailer, so a truncated download fails here instead of
    blowing up inside docker build's ADD step.
    """
    try:
        with tarfile.open(str(path)) as tf:
            return len(tf.getmembers()) > 0
    except Exception:
        # Any parse error (TarError, EOFError on truncated gzip, lzma errors)
        # means the file is unusable as the Qt source archive.
        return False


def qt_source_ok() -> bool:
    """Return True if a previously cached Qt source is plausible.

    Only a size guard here: the file went through is_qt_tarball() the moment it
    was downloaded, so re-scanning 25MB of tar headers on every invocation buys
    nothing.
    """
    return QT_SOURCE_TAR.is_file() and QT_SOURCE_TAR.stat().st_size > QT_SOURCE_MIN_SIZE


def remove_qt_source() -> None:
    """Best-effort removal; Python 3.6 lacks Path.unlink(missing_ok=True)."""
    try:
        QT_SOURCE_TAR.unlink()
    except FileNotFoundError:
        pass


def ensure_qt_source() -> None:
    """Download the Qt 5.6.3 qtbase source tarball if it is not cached locally."""
    if qt_source_ok():
        return
    QT_SOURCE_TAR.parent.mkdir(parents=True, exist_ok=True)
    urls = ([QT_SOURCE_MIRROR] if QT_SOURCE_MIRROR else []) + [
        # GitHub tag 归档直连实测最快（国内 ~250 KB/s）。
        "https://codeload.github.com/qt/qtbase/tar.gz/refs/tags/v5.6.3",
        # 官方归档兜底；该文件实测很慢（~12 KB/s），仅作最后备选。
        # Qt 5.6.3 已停止维护，清华/USTC/阿里云/华为云等国内镜像只同步
        # 5.15/6.x 版本，均无 5.6；如需自定义更快的源，用 QT_SOURCE_MIRROR。
        "https://download.qt.io/archive/qt/5.6/5.6.3/submodules/qtbase-opensource-src-5.6.3.tar.xz",
    ]
    for url in urls:
        logger.info("下载 Qt 5.6.3 源码: %s", url)
        cmd = [
            "curl", "-sL", "--fail",
            "--retry", "3", "--retry-delay", "2",
            "--connect-timeout", "20",
            # 传输阶段也要超时：download.qt.io 实测 12KB/s，挂起的连接
            # 若不加 max-time 会让脚本无限等待。
            "--max-time", "600",
            "-o", str(QT_SOURCE_TAR), url,
        ]
        if run(cmd, check=False).returncode == 0 \
                and QT_SOURCE_TAR.stat().st_size > QT_SOURCE_MIN_SIZE \
                and is_qt_tarball(QT_SOURCE_TAR):
            return
        remove_qt_source()
    logger.error("下载 Qt 5.6.3 源码失败，请手动放置到 %s", QT_SOURCE_TAR)
    sys.exit(1)


def build_image(no_cache: bool = False) -> None:
    ensure_qt_source()
    current_hash = compute_dockerfile_hash()
    stored_hash = image_dockerfile_hash()

    if not no_cache and stored_hash == current_hash:
        logger.info(
            "镜像 %s 与 Dockerfile 一致（hash=%s），跳过 docker build",
            IMAGE_NAME,
            current_hash[:12],
        )
        return
    if stored_hash and stored_hash != current_hash:
        logger.info(
            "Dockerfile 已变更（%s -> %s），重建 %s",
            stored_hash[:12],
            current_hash[:12],
            IMAGE_NAME,
        )
    elif not stored_hash and image_exists():
        logger.info("镜像 %s 缺少 hash 标签，重建镜像", IMAGE_NAME)

    cmd = [
        "docker",
        "build",
        "--network",
        "host",
        "--platform",
        "linux/amd64",
        "--label",
        "%s=%s" % (DOCKERFILE_HASH_LABEL, current_hash),
        "-f",
        str(DOCKERFILE_PATH),
        "-t",
        IMAGE_NAME,
        str(PC_ROOT),
    ]
    if no_cache:
        cmd.insert(2, "--no-cache")

    logger.info("构建 Docker 镜像 %s ...（首次需编译 Qt 5.6.3，耗时较长）", IMAGE_NAME)
    run(cmd)


def install_artifacts() -> None:
    for arch in ARCHES:
        dest_dir = DIST_DIR / arch
        dest_dir.mkdir(parents=True, exist_ok=True)
        for name in ARTIFACTS:
            src = DOCKER_BUILD_ROOT / name
            if not src.is_file():
                logger.error("未找到编译产出: %s", src)
                sys.exit(1)
            dest = dest_dir / name
            shutil.copy2(str(src), str(dest))
            os.chmod(str(dest), 0o755)
            logger.info("copy file: %s -> %s", src, dest)


def build_in_container() -> None:
    mounts = ["%s:/lafdup" % PC_ROOT]

    lafrpc_root = PROJECTS_ROOT / "lafrpc"
    if not (lafrpc_root / "cpp" / "lafrpc.h").is_file():
        # lafrpc 是必需依赖（无回退），缺失时在挂载前明确报错，
        # 而不是让容器内 add_subdirectory 给出不指明原因的 CMake 错误。
        logger.error(
            "未找到 lafrpc（%s/cpp/lafrpc.h）。lafdup 依赖 lafrpc，请在 lafdup 同级目录检出后再构建。",
            lafrpc_root,
        )
        sys.exit(1)
    # pc 以 /lafdup 挂载后，pc/CMakeLists.txt 的 lafrpc 探测会命中 else 分支
    # add_subdirectory(lafrpc/cpp lafrpc)，解析到 /lafdup/lafrpc/cpp/lafrpc.h，
    # 与 lafrpc 挂载点（/lafdup/lafrpc）一致。
    mounts.append("%s:/lafdup/lafrpc" % lafrpc_root)

    qtng_root = PROJECTS_ROOT / "qtng"
    if (qtng_root / "qt" / "CMakeLists.txt").is_file():
        # qtng 挂到 /lafdup/qtng：lafrpc 的 LAFRPC_USE_QTNG 分支从
        # /lafdup/lafrpc/cpp 出发探测 ../../qtng/qt，恰好命中挂载点。
        mounts.append("%s:/lafdup/qtng" % qtng_root)
        logger.info("检测到 qtng Qt binding（%s/qt），lafrpc 将自动切换为使用 qtng。", qtng_root)
    else:
        # 已弃用的独立 qtnetworkng 项目：若 lafdup 根仍保留其检出
        # （lafdup/qtnetworkng），单独挂载以保证容器内回退可用。
        qtnetworkng_root = LAFDUP_ROOT / "qtnetworkng"
        if (qtnetworkng_root / "qtnetworkng.h").is_file():
            mounts.append("%s:/lafdup/qtnetworkng" % qtnetworkng_root)
            logger.warning("未检测到 qtng（%s/qt/CMakeLists.txt），回退到独立的 qtnetworkng 项目。", qtng_root)
        else:
            logger.warning("未检测到 qtng（%s/qt/CMakeLists.txt）与独立 qtnetworkng，网络库可能缺失。", qtng_root)

    cmd = [
        "docker",
        "run",
        "--rm",
        "--platform",
        "linux/amd64",
    ]
    for mount in mounts:
        cmd.extend(["-v", mount])
    cmd.append(IMAGE_NAME)
    logger.info("在 mingw-w64 容器内交叉编译 WinXP PE，挂载 %d 个源码目录 ...", len(mounts))
    run(cmd)
    install_artifacts()


def main() -> None:
    parser = argparse.ArgumentParser(description="lafdup Windows XP mingw-w64 交叉编译")
    parser.add_argument(
        "--no-cache",
        action="store_true",
        help="强制无缓存重建 Docker 镜像",
    )
    args = parser.parse_args()

    check_docker()
    build_image(no_cache=args.no_cache)
    build_in_container()
    logger.info("完成。WinXP 可执行文件在 %s/win32/", DIST_DIR)


if __name__ == "__main__":
    main()
