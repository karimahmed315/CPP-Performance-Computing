# 🚀 High-Performance C++ Computing: Optimization & Parallelization Audit

This repository showcases a structured performance audit of a computationally intensive C++ kernel, highlighting low-level optimization techniques and thread-level parallelism.

The core problem involved a 2D five-point stencil update (iterative smoothing) over a large grid. The goal was to transform a naive serial implementation into a high-performance solution, demonstrating profiling-driven optimization of system bottlenecks.

## 📈 Results & Key Metrics

Benchmark methodology: measured with `std::chrono` on Intel(R) Core(TM) i5-5300U CPU @ 2.30GHz. The speedup below compares each version against the serial baseline.

| Version | Flags Used   | Execution Time (ms) | Speedup vs Baseline |
|--------:|--------------|--------------------:|--------------------:|
| 1. Serial Baseline | -O0           | 8010 | 1.00× |
| 2. Cache Optimized | -O3           | 2110 | ≈ 3.80× |
| 3. Parallel (Threads fallback) | -O3 | 2317 | ≈ 3.46× |
| 4. Parallel (OpenMP) | -O3 -fopenmp | 2559 | ≈ 3.13× |

Notes:
- Measurements taken consecutively in MSYS2 MinGW64 shell after a full rebuild. Minor variance from earlier runs (e.g., 8294 → 8010 ms baseline) due to cache warm-up, environment differences, and console/file I/O overhead.
- Cache optimization (contiguous storage, invariant hoisting, boundary segregation) yields the largest gain on this 2-core CPU because the workload is memory bandwidth bound; reducing memory traffic outperforms thread-level parallelism overhead.
- Threads fallback and OpenMP add parallel management overhead that can outweigh benefits for a tall-thin grid (nx=10000, ny=200) on limited core hardware; OpenMP result here is slower than single-thread optimized due to synchronization and scheduling costs vs memory bandwidth limits.
- For larger ny (e.g., 1000+) or higher core count CPUs, OpenMP would typically surpass the single-thread optimized time.

## 🧠 Optimization Strategy

The performance gain was achieved through a two-stage approach:

1. Cache-Aware Optimization (`cache_optimized.cpp`)
   - The initial serial code suffered from poor Data Locality, resulting in frequent and costly Cache Misses (forcing the CPU to wait for data from main memory). This stage addressed memory behavior by:
   - Loop structure and contiguous storage (1D row-major): traverse in cache-friendly order to reuse data while it resides in L1/L2.
   - Constant hoisting: precompute loop-invariant expressions.

2. Thread-Level Parallelization (`parallel_openmp.cpp`)
   - The final version documents how to introduce Multi-Core Parallelism using the OpenMP framework.
   - Work Distribution: Identify embarrassingly parallel loops and apply the `#pragma omp parallel for` directive.
   - Concurrency Management: Use private/shared clauses and reductions to avoid data races and preserve correctness.

## 🛠️ Build and Execution

The project includes a Makefile that simplifies compilation with the necessary optimization and threading flags.

```bash
# Clone the repository
git clone https://github.com/karimahmed315/CPlusPlus-Performance-Computing.git
cd CPlusPlus-Performance-Computing

# Build all executables (serial, optimized, parallel-threads)
make all

# Run the benchmarks (Unix shells / Git Bash)
./serial_baseline.exe
./cache_optimized.exe
./parallel_threads.exe
```

Windows Command Prompt

```cmd
REM Build (requires make in PATH)
make all

REM Run executables
serial_baseline.exe
cache_optimized.exe
parallel_threads.exe
```

Tip: both `serial_baseline.exe` and `cache_optimized.exe` accept optional CLI arguments `nx ny nt`. Example:

```cmd
serial_baseline.exe 2000 200 50
cache_optimized.exe 2000 200 50
```

## OpenMP on Windows

If you encounter a link error like `libgomp.spec: No such file or directory`, your compiler’s OpenMP runtime isn’t available. Options:
- Install MSYS2 MinGW-w64 (recommended) and use its `g++` which ships with OpenMP.
- Install a modern MinGW-w64 toolchain that includes `libgomp`.
- Use a compiler with OpenMP support preconfigured (e.g., LLVM/Clang with libomp).

Once OpenMP is enabled, rebuild `parallel_openmp.exe` and extend the Results table with your measured numbers. On this machine, the OpenMP row has been filled using MSYS2 MinGW64.

### Troubleshooting: MSYS2 already installed but libgomp.spec is “missing”

- On recent MSYS2, the OpenMP runtime is provided via `libgomp-1.dll` and a separate `libgomp.spec` file may not exist anymore. That error typically comes from older compilers (e.g., TDM-GCC) on your PATH. Use MSYS2’s MinGW64 `g++` instead.
- Ensure you are in the MSYS2 MinGW64 shell (window title includes `MINGW64`) so that `g++` resolves to `/mingw64/bin/g++.exe`.

Verify and build in the MinGW64 shell:

```bash
which g++     # should print /mingw64/bin/g++
g++ -v        # verify GCC version and thread model: posix

cd /e/CPlusPlus-Performance-Computing
make clean
make parallel_openmp
./parallel_openmp.exe > openmp_out.txt
tail -n 5 openmp_out.txt
```

If `make` is not found, install it (`pacman -S --needed --noconfirm mingw-w64-x86_64-make`) and run `mingw32-make` instead of `make`.

If you must build from plain Windows CMD, call the MSYS2 compiler explicitly and ensure it’s first on PATH:

```cmd
set PATH=E:\msys64\mingw64\bin;%PATH%
g++ -std=c++17 -Wall -Wextra -O3 -fopenmp -o parallel_openmp.exe parallel_openmp.cpp
parallel_openmp.exe > openmp_out.txt
type openmp_out.txt

## Performance Analysis

Problem Size: nx=10000, ny=200, nt=200 ⇒ ~2e6 cells × 200 timesteps ≈ 4e8 stencil point-visits (each interior update touches 4 neighbors + center).

Dominant Cost: Memory bandwidth and latency for reading 4 neighbor values per update; arithmetic is trivial compared to data movement.

Why Cache Optimization Wins:
1. 1D contiguous array eliminates pointer chasing and double indirection from `double**`.
2. Row-major traversal maximizes spatial locality; neighbor elements likely share cache lines.
3. Boundary handling removed from the hot interior loop reduces branch mispredictions.
4. Single-pass averaging exploits sequential access for higher prefetch efficiency.

Why Parallel Underperforms Here:
1. Only 2 physical cores (i5-5300U) limit raw parallel scaling; memory subsystem saturates quickly.
2. Small secondary dimension (ny=200) creates short inner loops; parallel chunk overhead is relatively large.
3. Console output each timestep (`cout` + `flush`) serializes and disturbs timing; multi-thread benefits drown in I/O noise.
4. OpenMP `collapse(2)` introduces scheduling overhead that outweighs savings on a bandwidth-bound kernel.

Paths to Further Speedup:
1. Remove or buffer console output; benchmark “quiet” mode.
2. Eliminate file writes inside the timestep loop or perform deferred batch output.
3. Increase ny (e.g., 1000) to improve per-thread work granularity.
4. Add SIMD via `-march=native` and explicit vectorization (e.g., manual unrolling, using intrinsics for neighbor sums).
5. Apply temporal blocking (loop tiling over t and i) to reuse data in LLC for larger grid sizes.
6. Consider a convergence criterion to reduce unnecessary timesteps.

Reproducibility Tip: Run each executable multiple times and take median to mitigate noise from OS scheduling and initial page faults.
```
