CXX = g++
CXX_FLAGS = -std=c++17 -Wall -Wextra

serial: serial_baseline.cpp
	$(CXX) $(CXX_FLAGS) -O0 -o serial_baseline.exe serial_baseline.cpp

optimized: cache_optimized.cpp
	$(CXX) $(CXX_FLAGS) -O3 -o cache_optimized.exe cache_optimized.cpp

# Fallback parallel (no OpenMP required)
parallel_threads: parallel_openmp.cpp
	$(CXX) $(CXX_FLAGS) -O3 -o parallel_threads.exe parallel_openmp.cpp

# Optional: OpenMP-enabled build (requires libgomp)
parallel_openmp: parallel_openmp.cpp
	$(CXX) $(CXX_FLAGS) -O3 -fopenmp -o parallel_openmp.exe parallel_openmp.cpp

all: serial optimized parallel_threads

clean:
	- rm -f serial_baseline.exe cache_optimized.exe parallel_threads.exe parallel_openmp.exe
	- cmd /c del /Q serial_baseline.exe cache_optimized.exe parallel_threads.exe parallel_openmp.exe 2>nul
