#include <admxrc2.h>

#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/program_options.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>


using std::exception;
using std::exit;
using std::string;
using filter_core::FPGACommunicator;
using filter_core::GrayscaledCamera;
using filter_core::GetOptions;


namespace filter_core {

constexpr double memory_clock_frequency = 66.67;
}  // namespace filter_core


void test(FPGACommunicator& communicator) {
  uint32_t src[32];
  std::iota(std::begin(src), std::end(src), 0);
  for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(src[i]) << " " ; }
  std::cout << std::endl << std::endl;

  communicator.write(src, 0, 32 * 4, 0);

  uint32_t dst[32];
  communicator.read(dst, 0, 32 * 4, 0);
  for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(dst[i]) << " " ; }
  std::cout << std::endl;
}

void test2(FPGACommunicator& communicator) {
  using namespace filter_core;

  uint32_t src[32];
  std::iota(std::begin(src), std::end(src), 0);
  std::cout << "src: ";
  for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(src[i]) << " " ; }
  std::cout << std::endl;

  communicator.write(src, 0, 32 * 4, 0);

  communicator.write(filter_core::IMAGE_SIZE_REG, 32);
  communicator.write(filter_core::REFLESH_REG, 1);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  communicator.write(filter_core::REFLESH_REG, 0);
  communicator.write(filter_core::ENABLE_REG, 1);

  std::cout << "waiting";
  for (int i = 0; i < 10 || communicator[filter_core::FINISH_REG] == 0; ++i) {
    std::cout << "." << std::flush;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  if (communicator[filter_core::FINISH_REG] > 0) {
    uint32_t dst[32];
    communicator.read(dst, 0, 32 * 4, 1);

    std::cout << "\rdst: " << std::flush;
    for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(dst[i]) << " " ; }
    std::cout << std::endl;
  } else {
    std::cerr << "failed: " << communicator[REFLESH_REG] << ", " <<
      communicator[IMAGE_SIZE_REG] << ", " << communicator[ENABLE_REG] << ", " << std::endl <<
      communicator[0x61] << ", " << communicator[0x62] << ", " <<
      communicator[0x63] << std::endl;
  }

  communicator.write(filter_core::ENABLE_REG, 0);
}

int main(int argc, char** argv) {
  try {
    auto options = GetOptions(argc, argv);

    if (options) {
      std::cout << "configuring..." << std::flush;
      auto communicator = FPGACommunicator(
          options->frequency,
          filter_core::memory_clock_frequency,
          options->filename);
      std::cout << "done" << std::endl;

      test(communicator);
      test2(communicator);

      const cv::Size image_size = options->image_size;
      const uint32_t total_size = image_size.area();
      const cv::Size double_size = {image_size.width * 2, image_size.height};

      communicator.write(filter_core::IMAGE_SIZE_REG, total_size);

      GrayscaledCamera camera(image_size);
      cv::Mat dst(image_size, CV_8UC1);

      cv::Mat combined(double_size, CV_8UC1);

      for (;;) {
        auto src = camera.get();

        communicator.write(src.data, 0, total_size, 0);

        communicator.write(filter_core::REFLESH_REG, 1);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        communicator.write(filter_core::REFLESH_REG, 0);

        for (int i = 0; i < 100 || communicator[filter_core::FINISH_REG] == 0; ++i) {
          std::this_thread::sleep_for(std::chrono::microseconds(250));
        }

        communicator.read(dst.data, 0, total_size, 1);

        cv::Mat left_roi(combined, cv::Rect(0, 0, image_size.width, image_size.height));
        src.copyTo(left_roi);
        cv::Mat right_roi(combined,
                          cv::Rect(image_size.width, 0, image_size.width, image_size.height));
        dst.copyTo(right_roi);

        cv::imshow("filter", combined);
        if(cv::waitKey(30) >= 0) break;
      }

      exit(EXIT_SUCCESS);
    } else {
      exit(EXIT_SUCCESS);
    }
  } catch(exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch(...) {
    std::cerr << "some exceptions were thrown" << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

