#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/framerate_checker.h"

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <iterator>
#include <thread>


using std::chrono::microseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using filter_core::ENABLE_REG;
using filter_core::IMAGE_SIZE_REG;
using filter_core::FPGACommunicator;
using filter_core::Camera;
using filter_core::SendRefresh;


namespace filter_core {

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

  communicator.write(IMAGE_SIZE_REG, 32);
  SendRefresh(communicator);
  communicator.write(ENABLE_REG, 1);

  std::cout << "waiting";
  for (int i = 0; i < 1000 && communicator[FINISH_REG] == 0; ++i) {
    std::cout << "." << std::flush;
    sleep_for(microseconds(250));
  }

  if (communicator[FINISH_REG] > 0) {
    uint32_t dst[32];
    communicator.read(dst, 0, 32 * 4, 1);

    std::cout << "\rdst: " << std::flush;
    for (int i = 0; i < 32; ++i) { std::cout << static_cast<uint32_t>(dst[i]) << " " ; }
    std::cout << std::endl;
  } else {
    std::cerr << "failed: " << communicator[REFRESH_REG] << ", " <<
      communicator[IMAGE_SIZE_REG] << ", " << communicator[ENABLE_REG] << ", " << std::endl <<
      communicator[0x61] << ", " << communicator[0x62] << ", " <<
      communicator[0x63] << std::endl;
  }

  communicator.write(ENABLE_REG, 0);
}

void testCam(cv::Size image_size, int interpolation) {
  auto start = system_clock::now();
 
    for (auto src :
         Camera(
             Converter(image_size, CV_8UC4, CV_BGR2BGRA, interpolation))) {
    FramerateChecker framerate_checker(start);

    cv::imshow("filter", src);
    if(cv::waitKey(10) >= 0) { break; }
  }
}

void test(FPGACommunicator& com, cv::Size size, int inter) {
/*      test(communicator);
      test(communicator);
      test(communicator);*/
  test2(com);
  test2(com);
  test2(com);
  testCam(size, inter);
}
}  // namespace filter_core

