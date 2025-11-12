/*
High-Performance C++: Naive serial implementation
Purpose: Baseline reference for correctness and performance comparisons.
Notes: Straightforward 2D five-point stencil update; no cache or parallel optimizations.
*/
#include <iostream>
#include <fstream>
#include <math.h> // potential poor practice: should use <cmath> for C++ compatibility
#include <chrono>
using namespace std;

int main(int argc, char* argv[]) {

        int nx(10000); //considered defining constants for readability purposes

        int ny(200);   //^^

        int nt(200);   //^^

        // Optional CLI: nx ny nt
        if (argc >= 4) {
                nx = atoi(argv[1]);
                ny = atoi(argv[2]);
                nt = atoi(argv[3]);
        }

        double** vi=new double*[nx]; // poor memory management: can cause memory leaks; use std::vector instead

        double** vr=new double*[nx]; //^^

        double pi=(4.*atan(1.)); // use of atan: consider using M_PI for clarity and precision

        for(int i=0;i<nx;i++) {

                vi[i]=new double[ny]; //allocating memory in a loop can lead to fragmentation (poor practice)

                vr[i]=new double[ny]; //^^

        }

        for (int i=0;i<nx;i++) {

                for (int j=0;j<ny;j++) {

                    vi[i][j] = double(i * i) * double(j) * sin(pi / double(nx) * double(i)); // calculations could be simplified more

                    vr[i][j] = 0.; //could use same loop as vi
                }
        }


        ofstream fout("data_out"); // file output inside loops slows performance, consider buffering output

        auto t_start = std::chrono::high_resolution_clock::now();
        for(int t=0;t<nt;t++) {

                cout<<"\n"<<t;cout.flush(); //inefficient output, frequent console output could slow execution

                for(int i=0;i<nx;i++) {

                        for(int j=0;j<ny;j++) { //complex boundary conditions could be simplified to reduce complexity

                                if(i>0&&i<nx-1&&j>0&&j<ny-1) {

                                        vr[i][j]=(vi[i+1][j]+vi[i-1][j]+vi[i][j-1]+vi[i][j+1])/4.;

                                } else if(i==0&&i<nx-1&&j>0&&j<ny-1) {

                                        vr[i][j]=(vi[i+1][j]+10.+vi[i][j-1]+vi[i][j+1])/4.;

                                } else if(i>0&&i==nx-1&&j>0&&j<ny-1) {

                                        vr[i][j]=(5.+vi[i-1][j]+vi[i][j-1]+vi[i][j+1])/4.;

                                } else if(i>0&&i<nx-1&&j==0&&j<ny-1) {

                                        vr[i][j]=(vi[i+1][j]+vi[i-1][j]+15.45+vi[i][j+1])/4.;

                                } else if(i>0&&i<nx-1&&j>0&&j==ny-1) {

                                        vr[i][j]=(vi[i+1][j]+vi[i-1][j]+vi[i][j-1]-6.7)/4.;

                                }
                        }
                }

                for(int i=0;i<nx;i++) {

                        for(int j=0;j<ny;j++) {

                                if(fabs(fabs(vr[i][j])-fabs(vi[i][j]))<1e-2) fout<<"\n"<<t<<" "<<i<<" "<<j<<" "<<fabs(vi[i][j])<<" "<<fabs(vr[i][j]);

                        }
                }

                for(int i=0;i<nx;i++) {

                        for(int j=0;j<ny;j++) vi[i][j]=vi[i][j]/2.+vr[i][j]/2.; //potential optimization: avg'ing a single pass

                }
        }
        auto t_end = std::chrono::high_resolution_clock::now();
        auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t_end - t_start).count();
        cout << "\n[chrono] time_ms=" << elapsed_ms << "\n";
}
