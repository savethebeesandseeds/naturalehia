#include <torch/cuda.h>
#include <torch/torch.h>
#include <torch/version.h>

#include <ATen/Context.h>
#include <ATen/Version.h>
#include <ATen/ops/cudnn_convolution.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
    if (!torch::cuda::is_available()) {
        std::cerr << "LibTorch cannot access CUDA\n";
        return EXIT_FAILURE;
    }

    // Returning a std::string crosses LibTorch's libstdc++ ABI boundary. A
    // mismatched _GLIBCXX_USE_CXX11_ABI setting fails here at link time.
    const std::string build_configuration = at::show_config();
    if (build_configuration.empty()) {
        std::cerr << "LibTorch returned an empty build configuration\n";
        return EXIT_FAILURE;
    }

    const auto options = torch::TensorOptions().dtype(torch::kFloat32).device(torch::kCUDA);
    const auto values = torch::arange(1.0, 5.0, options);
    const float result = values.sum().cpu().item<float>();
    if (std::abs(result - 10.0F) > 0.0001F) {
        std::cerr << "LibTorch CUDA tensor result mismatch: " << result << '\n';
        return EXIT_FAILURE;
    }

    const auto cudnn_version = at::globalContext().versionCuDNN();
    if (cudnn_version != 92000) {
        std::cerr << "Unexpected bundled cuDNN version: " << cudnn_version << '\n';
        return EXIT_FAILURE;
    }

    const auto convolution_input = torch::ones({1, 1, 4, 4}, options);
    const auto convolution_weight = torch::ones({1, 1, 3, 3}, options);
    const auto convolution_output = at::cudnn_convolution(
        convolution_input, convolution_weight, {0, 0}, {1, 1}, {1, 1}, 1, false, true, false);
    const float convolution_sum = convolution_output.sum().cpu().item<float>();
    if (convolution_output.sizes() != at::IntArrayRef({1, 1, 2, 2}) ||
        std::abs(convolution_sum - 36.0F) > 0.0001F) {
        std::cerr << "cuDNN convolution result mismatch: shape " << convolution_output.sizes()
                  << ", sum " << convolution_sum << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "LibTorch " << TORCH_VERSION << " CUDA tensor and cuDNN convolution passed on "
              << static_cast<unsigned int>(torch::cuda::device_count()) << " device(s), cuDNN "
              << cudnn_version << '\n';
    return EXIT_SUCCESS;
}
