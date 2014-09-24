#include <admxrc2.h>
#include <boost/optional.hpp>

#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"

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
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
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

int main(void) {
  try {
    const double lclkFreq = 40.0;
    const double mclkFreq = 66.67;
//    const string bitstream_filename = "memory-xrc2-2v6000.bit";
    const string bitstream_filename = "memory.bit";

    std::cout << "configuring..." << std::flush;
    auto communicator = FPGACommunicator(
        lclkFreq,
        mclkFreq,
        bitstream_filename);
    std::cout << "done" << std::endl;

//    test(communicator);
//    test2(communicator);

    const cv::Size image_size = {800, 600};
    const uint32_t total_size = 800 * 600;
    const cv::Size double_size = {1600, 600};

    communicator.write(filter_core::IMAGE_SIZE_REG, total_size);

    GrayscaledCamera camera(image_size);
    cv::Mat dst(image_size, CV_8UC1);

    cv::Mat combined(double_size, CV_8UC1);

    for (;;) {
      auto src = camera.get();

      communicator.write(src.data, 0, total_size, 0);

      communicator.write(filter_core::REFLESH_REG, 1);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      communicator.write(filter_core::REFLESH_REG, 0);

      for (int i = 0; i < 100 || communicator[filter_core::FINISH_REG] == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }

      communicator.read(dst.data, 0, total_size, 1);

      cv::Mat left_roi(combined, cv::Rect(0, 0, 800, 600));
      src.copyTo(left_roi);
      cv::Mat right_roi(combined, cv::Rect(800, 0, 800, 600));
      dst.copyTo(right_roi);

      cv::imshow("filter", combined);
      if(cv::waitKey(30) >= 0) break;
    }

    exit(EXIT_SUCCESS);
  } catch(exception& e) {
    std::cerr << std::endl << e.what() << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

