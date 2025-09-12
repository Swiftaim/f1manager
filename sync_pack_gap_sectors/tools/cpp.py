
import argparse, os, sys, subprocess, shutil

def run(cmd, cwd=None):
    print("> " + " ".join(cmd))
    r = subprocess.run(cmd, cwd=cwd)
    if r.returncode != 0:
        sys.exit(r.returncode)

def find_exe(target, build_dir, config):
    # Multi-config (MSVC): binary is under {build}/{config}/{target}.exe (or in target-specific dir)
    names = [target + ".exe", target + ".bat", target]  # widen search
    # Common search locations
    candidates = [
        os.path.join(build_dir, target + ".exe"),
        os.path.join(build_dir, config, target + ".exe"),
        os.path.join(build_dir, "bin", config, target + ".exe"),
        os.path.join(build_dir, "bin", target + ".exe"),
        os.path.join(build_dir, config, "bin", target + ".exe"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    # Fallback: walk a little
    for root, dirs, files in os.walk(build_dir):
        for n in names:
            if n in files:
                return os.path.join(root, n)
    return None

def main():
    parser = argparse.ArgumentParser(prog="cpp", description="CMake helper (build/test/run).")
    parser.add_argument("-S", "--source-dir", default=os.getcwd())
    parser.add_argument("-B", "--build-dir", default=os.path.join(os.getcwd(), "build"))
    parser.add_argument("-G", "--generator", default=None, help="CMake generator (optional)")
    parser.add_argument("-C", "--config", default="Debug", help="Build config: Debug/Release/RelWithDebInfo/MinSizeRel")

    sub = parser.add_subparsers(dest="cmd", required=True)

    p_build = sub.add_parser("build", help="Configure & build")
    p_build.add_argument("--target", default=None, help="Specific target to build")
    p_build.add_argument("--reconfigure", action="store_true", help="Force reconfigure (delete cache)")

    p_test = sub.add_parser("test", help="Run ctest")
    p_test.add_argument("--ctest-args", default="", help="Extra args for ctest")

    p_run = sub.add_parser("run", help="Run an executable target (default f1tm_app)")
    p_run.add_argument("--target", default="f1tm_app", help="Target name (default f1tm_app)")
    p_run.add_argument("args", nargs=argparse.REMAINDER, help="Arguments after '--' are passed to the app")

    args = parser.parse_args()

    os.makedirs(args.build_dir, exist_ok=True)

    # Configure (idempotent)
    cache = os.path.join(args.build_dir, "CMakeCache.txt")
    if args.cmd == "build" and args.reconfigure and os.path.exists(cache):
        os.remove(cache)

    if not os.path.exists(cache):
        cmake_cmd = ["cmake", "-S", args.source_dir, "-B", args.build_dir]
        if args.generator:
            cmake_cmd += ["-G", args.generator]
        run(cmake_cmd)

    if args.cmd == "build":
        b = ["cmake", "--build", args.build_dir, "--config", args.config]
        if args.target:
            b += ["--target", args.target]
        run(b)
        return

    if args.cmd == "test":
        # Use ctest -C for config on Windows
        t = ["ctest", "--test-dir", args.build_dir, "-C", args.config, "--output-on-failure"]
        if args.ctest_args:
            t += args.ctest_args.split()
        run(t)
        return

    if args.cmd == "run":
        # Ensure built
        run(["cmake", "--build", args.build_dir, "--config", args.config, "--target", args.target])

        exe = find_exe(args.target, args.build_dir, args.config)
        if not exe:
            print(f"error: could not find executable for target '{args.target}' under '{args.build_dir}' (config {args.config})", file=sys.stderr)
            sys.exit(1)

        # Pass through args after '--' if present
        app_argv = []
        if args.args:
            # Strip a leading '--' if the user provided it
            app_argv = args.args
            if app_argv and app_argv[0] == "--":
                app_argv = app_argv[1:]

        run([exe] + app_argv)
        return

if __name__ == "__main__":
    main()
