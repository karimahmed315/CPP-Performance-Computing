/*
High-Performance C++: OpenMP parallel implementation (with threads fallback)
Purpose: Parallelize the same 2D five-point stencil as the baseline using OpenMP; when OpenMP is unavailable,
         use a std::thread fallback with static row partitioning so we can still obtain parallel results.

OpenMP directives explained:
- #pragma omp parallel for schedule(static) : Divides loop iterations among threads evenly with minimal overhead.
- collapse(2) on nested loops: Flattens two loops into one iteration space to improve load balancing across threads.
- reduction, private/shared: Not required here (each iteration writes a unique element), but critical for dependent patterns.
*/
#include <iostream>
#include <fstream>
#include <cmath>
#include <chrono>
#include <vector>
#include <thread>
#include <algorithm>
#ifdef _OPENMP
#include <omp.h>
#endif

using namespace std;

static inline void stencil_update_block(
    const double* vi, double* vr, int nx, int ny, int i_begin, int i_end)
{
    // Update interior for rows [i_begin, i_end) (excludes boundary rows 0 and nx-1)
    for (int i = max(1, i_begin); i < min(nx - 1, i_end); ++i) {
        for (int j = 1; j < ny - 1; ++j) {
            vr[i * ny + j] = (vi[(i + 1) * ny + j] + vi[(i - 1) * ny + j] +
                              vi[i * ny + (j - 1)] + vi[i * ny + (j + 1)]) * 0.25;
        }
    }
}

int main(int argc, char* argv[]) {
    int nx = 10000;
    int ny = 200;
    int nt = 200;
    const double quarter = 0.25;
    const double half = 0.5;
    const double pi = 4.0 * atan(1.0);

    // Optional CLI: nx ny nt
    if (argc >= 4) {
        nx = atoi(argv[1]);
        ny = atoi(argv[2]);
        nt = atoi(argv[3]);
    }

    vector<double> vi(nx * ny), vr(nx * ny);

    // Initialize (row-major, cache-friendly)
    for (int i = 0; i < nx; ++i) {
        double i_sq = 1.0 * i * i;
        double i_factor = sin(pi / nx * i);
        for (int j = 0; j < ny; ++j) {
            vi[i * ny + j] = i_sq * j * i_factor;
            vr[i * ny + j] = 0.0;
        }
    }

    ofstream fout("data_out");
    if (!fout) {
        cerr << "Error opening output file.\n";
        return 1;
    }

    auto t_start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < nt; ++t) {
        cout << "\n" << t; cout.flush();

#ifdef _OPENMP
        // Interior update (OpenMP)
        // Parallelize nested loops; each thread writes to a unique (i,j) element.
        #pragma omp parallel for collapse(2) schedule(static)
        for (int i = 1; i < nx - 1; ++i) {
            for (int j = 1; j < ny - 1; ++j) {
                vr[i * ny + j] = (vi[(i + 1) * ny + j] + vi[(i - 1) * ny + j] +
                                  vi[i * ny + (j - 1)] + vi[i * ny + (j + 1)]) * quarter;
            }
        }
#else
        // Interior update (std::thread fallback)
        const unsigned hw = std::max(1u, std::thread::hardware_concurrency());
        const int num_threads = static_cast<int>(hw);
        vector<thread> threads;
        threads.reserve(num_threads);
        int rows = nx - 2; // interior rows [1, nx-2]
        int chunk = max(1, rows / num_threads);
        for (int tid = 0; tid < num_threads; ++tid) {
            int i_begin = 1 + tid * chunk;
            int i_end = (tid == num_threads - 1) ? (nx - 1) : min(nx - 1, i_begin + chunk);
            threads.emplace_back(stencil_update_block, vi.data(), vr.data(), nx, ny, i_begin, i_end);
        }
        for (auto& th : threads) th.join();
#endif

        // Boundaries (serial; small cost, keeps logic simple)
        for (int j = 1; j < ny - 1; ++j) {
            vr[0 * ny + j] = (vi[1 * ny + j] + 10.0 + vi[0 * ny + (j - 1)] + vi[0 * ny + (j + 1)]) * quarter;
            vr[(nx - 1) * ny + j] = (5.0 + vi[(nx - 2) * ny + j] +
                                     vi[(nx - 1) * ny + (j - 1)] + vi[(nx - 1) * ny + (j + 1)]) * quarter;
        }
        for (int i = 1; i < nx - 1; ++i) {
            vr[i * ny + 0] = (vi[(i + 1) * ny + 0] + vi[(i - 1) * ny + 0] + 15.45 + vi[i * ny + 1]) * quarter;
            vr[i * ny + (ny - 1)] = (vi[(i + 1) * ny + (ny - 1)] + vi[(i - 1) * ny + (ny - 1)] +
                                     vi[i * ny + (ny - 2)] - 6.7) * quarter;
        }

        // Conditional output (serial)
        for (int i = 0; i < nx; ++i) {
            for (int j = 0; j < ny; ++j) {
                if (fabs(fabs(vr[i * ny + j]) - fabs(vi[i * ny + j])) < 1e-2) {
                    fout << t << " " << i << " " << j << " "
                         << fabs(vi[i * ny + j]) << " " << fabs(vr[i * ny + j]) << "\n";
                }
            }
        }

        // Average update vi = (vi + vr)/2
#ifdef _OPENMP
        #pragma omp parallel for schedule(static)
        for (int idx = 0; idx < nx * ny; ++idx) {
            vi[idx] = (vi[idx] + vr[idx]) * half;
        }
#else
        const unsigned hw2 = std::max(1u, std::thread::hardware_concurrency());
        const int num_threads2 = static_cast<int>(hw2);
        vector<thread> threads2;
        threads2.reserve(num_threads2);
        int total = nx * ny;
        int block = max(1, total / num_threads2);
        auto avg_block = [&](int begin, int end){
            for (int k = begin; k < end; ++k) vi[k] = (vi[k] + vr[k]) * half;
        };
        for (int tid = 0; tid < num_threads2; ++tid) {
            int begin = tid * block;
            int end = (tid == num_threads2 - 1) ? total : min(total, begin + block);
            threads2.emplace_back(avg_block, begin, end);
        }
        for (auto& th : threads2) th.join();
#endif
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    cout << "\n[chrono] time_ms=" << elapsed_ms << "\n";

    return 0;
}

