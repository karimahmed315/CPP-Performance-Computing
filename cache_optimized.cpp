/*
High-Performance C++: Cache-optimized implementation
Purpose: Same algorithm as the serial baseline with improved data locality and reduced memory overhead.
Key ideas: 1D contiguous storage (row-major), constant hoisting, explicit boundary handling, and single-pass updates.
*/
#include <iostream>  //for console output
#include <fstream>   //for file output
#include <cmath>     //for mathematical operations
#include <chrono>

using namespace std;

int main(int argc, char* argv[]) {
    int nx = 10000; //grid size (x)
    int ny = 200;   //grid size (y)
    int nt = 200;   //num of time steps
    const double quarter = 0.25; // precomputed constants(replaced /4.0 with *quarter to improve speed)
    const double half = 0.5;     // ^^(replaced /2.0 with *half ^^)
    const double pi = 4.0 * atan(1.0); // portable pi calculation without relying on non-standard M_PI

    // Optional CLI: nx ny nt
    if (argc >= 4) {
        nx = atoi(argv[1]);
        ny = atoi(argv[2]);
        nt = atoi(argv[3]);
    }

    //allocate memory using new
    double* vi = new double[nx * ny]; //to store input vals
    double* vr = new double[nx * ny]; //to store results

    // initialize vi and vr arrays
    // Rationale: Iterate in row-major order (i outer, j inner) and use contiguous indexing (i*ny + j)
    // to exploit spatial locality. Precompute i*i and sin(pi/nx*i) once per row to reduce redundant work.
    for (int i = 0; i < nx; ++i) {
        double i_sq = i * i, i_factor = sin(pi / nx * i); // combined initialization
        for (int j = 0; j < ny; ++j) {
            vi[i * ny + j] = i_sq * j * i_factor; // initialize vi
            vr[i * ny + j] = 0.0;                //initialize vr to 0
        }
    }

    ofstream fout("data_out"); //for writing results
    if (!fout) {
        cerr << "Error opening output file.\n";
        return 1; //terminate if file cannot be opened
    }

    // iterate over time steps
    auto t_start = std::chrono::high_resolution_clock::now();
    for (int t = 0; t < nt; ++t) {
        cout << "\n" << t; cout.flush(); //print current time step to console

        // update vr based on vi (interior points)
        // Improves L1/L2 cache locality by traversing contiguous memory and removing pointer indirections.
        // The five-point stencil reuses neighboring elements that are likely resident in cache lines.
        for (int i = 1; i < nx - 1; ++i) {
            for (int j = 1; j < ny - 1; ++j) {
                vr[i * ny + j] = (vi[(i + 1) * ny + j] + vi[(i - 1) * ny + j] +
                                  vi[i * ny + (j - 1)] + vi[i * ny + (j + 1)]) * quarter;
            }
        }

        // handle boundary conditions explicitly (edges)
        // Move branches out of the hot interior loop to reduce branch mispredictions and keep the core loop tight.
        for (int j = 1; j < ny - 1; ++j) { //for boundary rows
            vr[0 * ny + j] = (vi[1 * ny + j] + 10.0 + vi[0 * ny + (j - 1)] + vi[0 * ny + (j + 1)]) * quarter; //top row
            vr[(nx - 1) * ny + j] = (5.0 + vi[(nx - 2) * ny + j] +
                                     vi[(nx - 1) * ny + (j - 1)] + vi[(nx - 1) * ny + (j + 1)]) * quarter; //bottom row
        }
        for (int i = 1; i < nx - 1; ++i) { //for boundary columns
            vr[i * ny + 0] = (vi[(i + 1) * ny + 0] + vi[(i - 1) * ny + 0] + 15.45 + vi[i * ny + 1]) * quarter; //left column
            vr[i * ny + (ny - 1)] = (vi[(i + 1) * ny + (ny - 1)] + vi[(i - 1) * ny + (ny - 1)] +
                                     vi[i * ny + (ny - 2)] - 6.7) * quarter; //right column
        }

        // output results for specific conditions
        for (int i = 0; i < nx; ++i) {
            for (int j = 0; j < ny; ++j) {
                if (fabs(fabs(vr[i * ny + j]) - fabs(vi[i * ny + j])) < 1e-2) { //check if within threshold
                    fout << t << " " << i << " " << j << " " << fabs(vi[i * ny + j]) << " " << fabs(vr[i * ny + j]) << "\n"; //write to file
                }
            }
        }

        // update vi array in a single loop
        // Single pass over contiguous memory improves bandwidth utilization vs nested loops.
        for (int i = 0; i < nx * ny; ++i) {
            vi[i] = (vi[i] + vr[i]) * half; //average with vr
        }
    }
    auto t_end = std::chrono::high_resolution_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
    cout << "\n[chrono] time_ms=" << elapsed_ms << "\n";
    // clean up memory
    delete[] vi; // release allocated memory
    delete[] vr; // ^^
    return 0;
}
