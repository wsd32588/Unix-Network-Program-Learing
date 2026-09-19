#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import re
import subprocess
import sys
import tempfile
from collections.abc import Mapping
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
WINDOWS_BUILD_CANDIDATES = (
    PROJECT_ROOT / "out" / "build" / "x64-Debug",
    PROJECT_ROOT / "cmake-build-debug-visual-studio",
    PROJECT_ROOT / "cmake-build-debug",
)


def run(
    command: list[str],
    environment: Mapping[str, str] | None = None,
) -> None:
    print("+", subprocess.list2cmdline(command), flush=True)
    subprocess.run(
        command,
        cwd=PROJECT_ROOT,
        check=True,
        env=environment,
    )


def find_windows_build_dir(requested: Path | None) -> Path:
    if requested is not None:
        build_dir = requested
        if not build_dir.is_absolute():
            build_dir = PROJECT_ROOT / build_dir
        if not (build_dir / "CMakeCache.txt").is_file():
            raise FileNotFoundError(
                f"CMake build directory is not configured: {build_dir}"
            )
        return build_dir

    for build_dir in WINDOWS_BUILD_CANDIDATES:
        if (build_dir / "CMakeCache.txt").is_file():
            return build_dir

    raise FileNotFoundError(
        "No configured Windows CMake build directory was found. "
        "Pass one with --build-dir."
    )


def find_msvc_environment(build_dir: Path) -> dict[str, str]:
    cache = (build_dir / "CMakeCache.txt").read_text(
        encoding="utf-8",
        errors="replace",
    )
    match = re.search(
        r"^CMAKE_CXX_COMPILER(?::[^=]+)?=(.+)$",
        cache,
        re.MULTILINE,
    )
    if match is None:
        raise RuntimeError("CMAKE_CXX_COMPILER is missing from CMakeCache.txt")

    compiler = Path(match.group(1).strip())
    vc_directory = next(
        (
            parent
            for parent in compiler.parents
            if parent.name.casefold() == "vc"
        ),
        None,
    )
    if vc_directory is None:
        raise RuntimeError(f"Cannot locate the Visual C++ directory from {compiler}")

    vcvars = vc_directory / "Auxiliary" / "Build" / "vcvars64.bat"
    if not vcvars.is_file():
        raise FileNotFoundError(f"MSVC environment script not found: {vcvars}")

    result = subprocess.run(
        f'call "{vcvars}" >nul && set',
        check=True,
        capture_output=True,
        shell=True,
        text=True,
    )

    environment = dict(os.environ)
    for line in result.stdout.splitlines():
        if not line or line.startswith("=") or "=" not in line:
            continue
        name, value = line.split("=", 1)
        environment[name] = value
    return environment


def run_windows_tests(build_dir: Path) -> None:
    environment = find_msvc_environment(build_dir)
    run([
        "cmake",
        "--build",
        str(build_dir),
        "--target",
        "process_tests",
        "--config",
        "Debug",
    ], environment)
    run([
        "ctest",
        "--test-dir",
        str(build_dir),
        "-C",
        "Debug",
        "-R",
        "^process_tests$",
        "--output-on-failure",
    ], environment)


def get_wsl_project_root() -> str:
    windows_path = str(PROJECT_ROOT).replace("\\", "/")
    result = subprocess.run(
        ["wsl.exe", "wslpath", "-a", "-u", windows_path],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.strip()


def run_wsl_tests() -> None:
    wsl_root = get_wsl_project_root()
    binary = "/tmp/unp_process_tests"
    run([
        "wsl.exe",
        "g++",
        "-std=c++23",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        f"-I{wsl_root}/include",
        f"{wsl_root}/tests/test_process.cpp",
        "-o",
        binary,
    ])
    run(["wsl.exe", binary])


def run_native_linux_tests() -> None:
    binary = Path(tempfile.gettempdir()) / "unp_process_tests"
    run([
        "g++",
        "-std=c++23",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        f"-I{PROJECT_ROOT / 'include'}",
        str(PROJECT_ROOT / "tests" / "test_process.cpp"),
        "-o",
        str(binary),
    ])
    run([str(binary)])


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build and run process_tests on the available platforms."
    )
    parser.add_argument(
        "--build-dir",
        type=Path,
        help="Configured Windows CMake build directory.",
    )
    parser.add_argument(
        "--skip-windows",
        action="store_true",
        help="Skip the Windows CMake/CTest run.",
    )
    parser.add_argument(
        "--skip-wsl",
        action="store_true",
        help="Skip the WSL/GCC run.",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    try:
        if os.name == "nt":
            if not arguments.skip_windows:
                run_windows_tests(
                    find_windows_build_dir(arguments.build_dir)
                )
            if not arguments.skip_wsl:
                run_wsl_tests()
        else:
            run_native_linux_tests()
    except (
        FileNotFoundError,
        RuntimeError,
        subprocess.CalledProcessError,
    ) as error:
        print(f"process test run failed: {error}", file=sys.stderr)
        return 1

    print("process tests passed", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
