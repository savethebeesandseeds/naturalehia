#include <cuda_runtime.h>

#include <cstdlib>
#include <iostream>

namespace {

constexpr int kExpectedCudaMajor = 13;
constexpr int kExpectedCudaMinor = 1;

constexpr int cuda_major(int version) { return version / 1000; }

constexpr int cuda_minor(int version) { return (version % 1000) / 10; }

constexpr bool same_cuda_release(int lhs, int rhs) {
    return cuda_major(lhs) == cuda_major(rhs) && cuda_minor(lhs) == cuda_minor(rhs);
}

static_assert(cuda_major(CUDART_VERSION) == kExpectedCudaMajor &&
                  cuda_minor(CUDART_VERSION) == kExpectedCudaMinor,
              "The Naturalehia GPU smoke test must be compiled with the "
              "CUDA 13.1 toolkit");

__global__ void double_value(int* value) { *value *= 2; }

bool cuda_ok(cudaError_t result, const char* operation) {
    if (result == cudaSuccess) {
        return true;
    }
    std::cerr << operation << ": " << cudaGetErrorString(result) << '\n';
    return false;
}

void print_cuda_version(std::ostream& stream, int version) {
    stream << cuda_major(version) << '.' << cuda_minor(version) << " (encoded " << version << ')';
}

} // namespace

int main() {
    int runtime_version = 0;
    if (!cuda_ok(cudaRuntimeGetVersion(&runtime_version), "cudaRuntimeGetVersion")) {
        return EXIT_FAILURE;
    }

    int driver_version = 0;
    if (!cuda_ok(cudaDriverGetVersion(&driver_version), "cudaDriverGetVersion")) {
        return EXIT_FAILURE;
    }

    if (cuda_major(runtime_version) != kExpectedCudaMajor ||
        cuda_minor(runtime_version) != kExpectedCudaMinor) {
        std::cerr << "CUDA runtime mismatch: expected CUDA " << kExpectedCudaMajor << '.'
                  << kExpectedCudaMinor << ", but loaded ";
        print_cuda_version(std::cerr, runtime_version);
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    if (!same_cuda_release(runtime_version, CUDART_VERSION)) {
        std::cerr << "CUDA toolkit/runtime mismatch: compiled against ";
        print_cuda_version(std::cerr, CUDART_VERSION);
        std::cerr << ", but loaded ";
        print_cuda_version(std::cerr, runtime_version);
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    // CUDA 13.x supports minor-version compatibility within the 13.x driver
    // family, so a driver reporting CUDA 13.0 is sufficient for this 13.1
    // smoke test. A newer major driver is compatible as well.
    if (cuda_major(driver_version) < cuda_major(runtime_version)) {
        std::cerr << "Incompatible NVIDIA driver: CUDA 13.1 requires a "
                     "driver supporting at least the CUDA 13.x family, but "
                     "the driver reports ";
        print_cuda_version(std::cerr, driver_version);
        std::cerr << '\n';
        return EXIT_FAILURE;
    }

    int device_count = 0;
    if (!cuda_ok(cudaGetDeviceCount(&device_count), "cudaGetDeviceCount") || device_count < 1) {
        std::cerr << "No CUDA device is available\n";
        return EXIT_FAILURE;
    }

    cudaDeviceProp properties{};
    if (!cuda_ok(cudaGetDeviceProperties(&properties, 0), "cudaGetDeviceProperties")) {
        return EXIT_FAILURE;
    }

    int* device_value = nullptr;
    if (!cuda_ok(cudaMalloc(&device_value, sizeof(int)), "cudaMalloc")) {
        return EXIT_FAILURE;
    }

    const int input = 21;
    int output = 0;
    bool succeeded = cuda_ok(cudaMemcpy(device_value, &input, sizeof(int), cudaMemcpyHostToDevice),
                             "cudaMemcpy host-to-device");
    if (succeeded) {
        double_value<<<1, 1>>>(device_value);
        succeeded = cuda_ok(cudaGetLastError(), "kernel launch") &&
                    cuda_ok(cudaDeviceSynchronize(), "cudaDeviceSynchronize") &&
                    cuda_ok(cudaMemcpy(&output, device_value, sizeof(int), cudaMemcpyDeviceToHost),
                            "cudaMemcpy device-to-host");
    }
    const bool freed = cuda_ok(cudaFree(device_value), "cudaFree");

    if (!succeeded || !freed || output != 42) {
        std::cerr << "CUDA smoke-test result mismatch\n";
        return EXIT_FAILURE;
    }

    std::cout << "CUDA kernel passed on " << properties.name << " (compute " << properties.major
              << '.' << properties.minor << "; toolkit/runtime " << cuda_major(CUDART_VERSION)
              << '.' << cuda_minor(CUDART_VERSION) << "; driver API " << cuda_major(driver_version)
              << '.' << cuda_minor(driver_version) << ")\n";
    return EXIT_SUCCESS;
}
