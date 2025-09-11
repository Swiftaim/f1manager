#!/usr/bin/env python3
"""
cpp: tiny CMake helper, TDD-friendly.

Auto-detects multi-config generators (Visual Studio, Xcode, Ninja Multi-Config).
Uses:
  - cmake --build [--config <Cfg>] when multi-config
  - ctest -C <Cfg> (only when multi-config) and runs inside build dir

Usage:
  cpp configure
  cpp build [--target <t>] [--config Debug]
  cpp test [--regex <re>] [--direct] [--exe-target <t>] [-- ...args]
  cpp run --target <t> [--args "<args>"] [--config Debug]
  cpp run --path <exe> [--args "<args>"]
  cpp clean
  cpp info
"""
import argparse, os, shutil, subprocess, sys, shlex
from pathlib import Path

GENERATOR_DEFAULT = os.environ.get("CMAKE_GENERATOR", "Visual Studio 17 2022")
ARCH_DEFAULT = os.environ.get("CMAKE_ARCH", "x64")
BUILD_DIR_DEFAULT = os.environ.get("CMAKE_BUILD_DIR", "build")
CONFIG_DEFAULT = os.environ.get("CMAKE_CONFIG", "Debug")

def is_multi_config(generator: str) -> bool:
    g = (generator or "").lower()
    return ("visual studio" in g) or ("xcode" in g) or ("multi-config" in g)

def run(cmd, cwd=None):
    print("> " + " ".join(cmd) + (f"    (cwd: {cwd})" if cwd else ""))
    r = subprocess.run(cmd, cwd=cwd)
    if r.returncode != 0:
        sys.exit(r.returncode)

def ensure_configured(source_dir: Path, build_dir: Path, generator: str, arch: str):
    if (build_dir / "CMakeCache.txt").exists():
        return
    build_dir.mkdir(parents=True, exist_ok=True)
    cmake_cmd = ["cmake", "-S", str(source_dir), "-B", str(build_dir), "-G", generator]
    if "visual studio" in generator.lower():
        cmake_cmd += ["-A", arch]
    run(cmake_cmd)

def cmake_build(build_dir: Path, config: str, target: str | None, multi_cfg: bool):
    cmd = ["cmake", "--build", str(build_dir)]
    if multi_cfg:
        cmd += ["--config", config]
    if target:
        cmd += ["--target", target]
    run(cmd)

def ctest(build_dir: Path, config: str, regex: str | None, multi_cfg: bool):
    # Run inside the build dir; older CTest supports -C, not --config/--test-dir
    cmd = ["ctest", "--output-on-failure"]
    if multi_cfg:
        cmd += ["-C", config]   # <-- correct flag for CTest
    if regex:
        cmd += ["-R", regex]
    run(cmd, cwd=build_dir)

def find_executable(build_dir: Path, config: str, target: str, multi_cfg: bool) -> Path | None:
    candidates = []
    if multi_cfg:
        candidates += [
            build_dir / config / f"{target}.exe",
            build_dir / "tests" / config / f"{target}.exe",
            build_dir / config / f"{target}",
            build_dir / "tests" / config / f"{target}",
        ]
    else:
        candidates += [
            build_dir / f"{target}.exe",
            build_dir / "tests" / f"{target}.exe",
            build_dir / target,
            build_dir / "tests" / target,
        ]
    for p in candidates:
        if p.exists():
            return p
    hits = list(build_dir.rglob(f"{target}.exe")) + list(build_dir.rglob(target))
    if not hits:
        return None
    if multi_cfg:
        for h in hits:
            if config.lower() in str(h.parent).lower():
                return h
    return hits[0]

def run_exe(path: Path, extra_args: list[str]):
    if not path.exists():
        print(f"ERROR: executable not found: {path}", file=sys.stderr)
        sys.exit(1)
    run([str(path)] + extra_args, cwd=path.parent)

def main():
    parser = argparse.ArgumentParser(prog="cpp", description="CMake convenience wrapper")
    parser.add_argument("--build-dir", default=BUILD_DIR_DEFAULT)
    parser.add_argument("--generator", default=GENERATOR_DEFAULT)
    parser.add_argument("--arch", default=ARCH_DEFAULT)
    parser.add_argument("--config", default=CONFIG_DEFAULT)

    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("configure")
    p_build = sub.add_parser("build"); p_build.add_argument("--target")
    p_test = sub.add_parser("test")
    p_test.add_argument("--regex"); p_test.add_argument("--direct", action="store_true")
    p_test.add_argument("--exe-target", default="f1tm_tests")
    p_test.add_argument("--", dest="double_dash", nargs=argparse.REMAINDER)
    p_run = sub.add_parser("run")
    p_run.add_argument("--target"); p_run.add_argument("--path")
    p_run.add_argument("--args", default="")
    sub.add_parser("clean"); sub.add_parser("info")
    args, _ = parser.parse_known_args()

    source_dir = Path.cwd()
    build_dir = (source_dir / args.build_dir).resolve()
    multi_cfg = is_multi_config(args.generator)

    if args.cmd == "info":
        print(f"Source: {source_dir}")
        print(f"Build : {build_dir}")
        print(f"Gen   : {args.generator}")
        print(f"Arch  : {args.arch}")
        print(f"Config: {args.config}")
        print(f"Multi : {multi_cfg}")
        return

    if args.cmd == "clean":
        if build_dir.exists():
            shutil.rmtree(build_dir)
            print(f"Removed {build_dir}")
        else:
            print(f"No build dir at {build_dir}")
        return

    if args.cmd == "configure":
        ensure_configured(source_dir, build_dir, args.generator, args.arch)
        return

    ensure_configured(source_dir, build_dir, args.generator, args.arch)

    if args.cmd == "build":
        cmake_build(build_dir, args.config, args.target, multi_cfg)
        return

    if args.cmd == "test":
        if args.direct:
            cmake_build(build_dir, args.config, args.exe_target, multi_cfg)
            exe = find_executable(build_dir, args.config, args.exe_target, multi_cfg)
            if not exe:
                sys.exit(f"ERROR: test executable for target '{args.exe_target}' not found")
            extra = args.double_dash if args.double_dash else []
            run_exe(exe, extra)
        else:
            ctest(build_dir, args.config, args.regex, multi_cfg)
        return

    if args.cmd == "run":
        if args.path:
            exe = Path(args.path)
        else:
            if not args.target:
                sys.exit("ERROR: need --target or --path")
            cmake_build(build_dir, args.config, args.target, multi_cfg)
            exe = find_executable(build_dir, args.config, args.target, multi_cfg)
            if not exe:
                sys.exit(f"ERROR: executable for target '{args.target}' not found")
        extra_args = shlex.split(args.args)
        run_exe(exe, extra_args)
        return

if __name__ == "__main__":
    main()
