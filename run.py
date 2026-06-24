#!/usr/bin/env python3
"""Run all FFT benchmark executables found in build/.

Mirrors run.sh. Usage:
    run.py [--avx512] [--runs N] <benchmark-args...>
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path


def positive_int(value):
    ivalue = int(value)
    if ivalue < 1:
        raise argparse.ArgumentTypeError(f"{value} is not a positive integer")
    return ivalue


def parse_args(argv):
    parser = argparse.ArgumentParser(
        prog="run.py",
        description="Run all FFT benchmark executables found in build/.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="All remaining arguments are forwarded to the benchmark executable.",
    )
    parser.add_argument(
        "--avx512",
        action="store_true",
        help="run avx512 builds instead of avx2 builds",
    )
    parser.add_argument(
        "--runs",
        type=positive_int,
        default=1,
        metavar="N",
        help="repeat all benchmarks N times, saving to results_<tag>_<run>.json",
    )
    parser.add_argument(
        "bench_args",
        nargs=argparse.REMAINDER,
        help="arguments forwarded to the benchmark executable",
    )
    args = parser.parse_args(argv)

    if not args.bench_args:
        parser.error("at least one benchmark argument is required")

    return args.avx512, args.runs, args.bench_args


def main():
    use_avx512, runs, bench_args = parse_args(sys.argv[1:])

    suffix = ".exe" if os.name == "nt" else ""
    data_dir = Path("data")
    data_dir.mkdir(parents=True, exist_ok=True)

    found = False

    for run in range(1, runs + 1):
        if runs > 1:
            print("################################")
            print(f"Run {run}/{runs}")
            print("################################")

        for bench in sorted(Path("build").glob(f"fft_benchmark_*{suffix}")):
            if not bench.is_file():
                continue

            # On *nix, skip if not executable
            if os.name != "nt":
                if not os.access(bench, os.X_OK):
                    print(f"Warning: {bench} is not executable. Skipping.")
                    continue

            # Extract the library+arch tag (e.g. "kfr-any" or "kissfft-avx2")
            libtag = bench.name[len("fft_benchmark_"):]
            # Strip .exe suffix on Windows
            if libtag.endswith(".exe"):
                libtag = libtag[:-4]

            # Skip based on --avx512 flag
            if use_avx512:
                # Only run -any or -avx512, skip -avx2
                if libtag.endswith("-avx2"):
                    print(f"Skipping {libtag} (--avx512 set, ignoring avx2 builds)")
                    continue
            else:
                # Only run -any or -avx2, skip -avx512
                if "-avx512" in libtag:
                    print(f"Skipping {libtag} (--avx512 not set, ignoring avx512 builds)")
                    continue

            outfile = data_dir / f"results_{libtag}_{run}.json"

            # Skip if json file already exists
            if outfile.exists():
                print(f"Results for {libtag} run {run} already exist. Skipping benchmark.")
                print(f"To rerun, delete {outfile} and run this script again.")
                continue

            print("================================")
            print(f"Running benchmark for {libtag} (run {run}/{runs})...")

            cmd = [str(bench), "--save", str(outfile), "--no-banner", *bench_args]
            subprocess.run(cmd, check=True)
            found = True

    if not found:
        print("Warning: No benchmark executables found in build/")


if __name__ == "__main__":
    main()
