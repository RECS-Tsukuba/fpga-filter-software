#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/program_options.h"

#include <admxrc2.h>
#include <boost/timer/timer.hpp>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>


using std::bind;
using std::exception;
using std::exit;
using std::string;
using std::to_string;
using cv::Rect;
using filter_core::FPGACommunicator;
using filter_core::GrayscaledCamera;
using filter_core::GetOptions;


namespace filter_core {

constexpr double memory_clock_frequency = 66.67;
constexpr const char* const frame_title = "filter";
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
  std::this_thread::sleep_for(std::chrono::microseconds(200));
  communicator.write(filter_core::REFLESH_REG, 0);
  communicator.write(filter_core::ENABLE_REG, 1);

  std::cout << "waiting";
  for (int i = 0; i < 1000 && communicator[filter_core::FINISH_REG] == 0; ++i) {
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


namespace {

class FramerateChecker {
 private:
  boost::timer::cpu_timer timer_;
 public:
  FramerateChecker() : timer_() {}
  ~FramerateChecker() {
    std::cout << "\r" << (1000 / (timer_.elapsed().wall / 1000 / 1000)) <<
      "fps" << std::flush;
  }
 private:
  FramerateChecker(const FramerateChecker&) = delete;
  FramerateChecker(FramerateChecker&&) = delete;
  FramerateChecker& operator=(const FramerateChecker&) = delete;
  FramerateChecker& operator=(FramerateChecker&&) = delete;
};

cv::Mat Combine(cv::Mat dst, cv::Mat filtered, cv::Mat original,
             uint64_t width, uint64_t height) {
  filtered.copyTo(cv::Mat(dst, Rect(0, 0, width, height)));
  original.copyTo(cv::Mat(dst, Rect(width, 0, width, height)));

  return dst;
}

void SendRefresh(FPGACommunicator& com) {
  com.write(filter_core::REFLESH_REG, 1);
  std::this_thread::sleep_for(std::chrono::microseconds(200));
  com.write(filter_core::REFLESH_REG, 0);
  std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void ShowWithCapturedFrame(cv::Mat dst, cv::Mat src, cv::Mat combined,
                           uint64_t width, uint64_t height) {
  cv::imshow(filter_core::frame_title,
             ::Combine(combined, dst, src, width, height));
}

void Show(
    cv::Mat dst, cv::Mat src, cv::Mat combined,
    cv::Size size,
    bool is_combined) {
  (is_combined)?
      ::ShowWithCapturedFrame(dst, src, combined, size.width, size.height) :
      cv::imshow(filter_core::frame_title, dst);
}
}  // namespace

void testCam(cv::Size image_size, int interpolation) {
  for (auto src : GrayscaledCamera(image_size, interpolation)) {
    ::FramerateChecker checker;
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
//      testCam(image_size, options->interpolation);
}

int main(int argc, char** argv) {
  try {
    auto options = GetOptions(argc, argv);

    if (options) {
      const cv::Size image_size = options->image_size;
      const uint32_t total_size = image_size.area();
      const cv::Size double_size = {image_size.width * 2, image_size.height};

      cv::Mat dst(image_size, CV_8UC1);
      cv::Mat combined(double_size, CV_8UC1);

      const string output_prefix("output");
      uint64_t output_counter = 0;

      std::cout << "configuring..." << std::flush;
      auto communicator = FPGACommunicator(
          options->frequency,
          filter_core::memory_clock_frequency,
          options->filename,
          total_size);
      std::cout << "done" << std::endl;

      communicator.write(filter_core::IMAGE_SIZE_REG, total_size);

      for (auto src : GrayscaledCamera(image_size, options->interpolation)) {
        ::FramerateChecker framerate_checker;

        communicator.write(src.data, 0, total_size, 0);
        ::SendRefresh(communicator);
        communicator.write(filter_core::ENABLE_REG, 1);

        for (int i = 0; i < 1000 && communicator[filter_core::FINISH_REG] == 0; ++i) {
          std::this_thread::sleep_for(std::chrono::microseconds(250));
        }
        communicator.write(filter_core::ENABLE_REG, 0);
        communicator.read(dst.data, 0, total_size, 1);

        Show(dst, src, combined, image_size, options->is_with_captured);

        auto key = cv::waitKey(30);
        if (key == 'p' || key == 'P') {
          cv::imwrite(
              output_prefix + to_string(output_counter) + ".png", dst);

          ++output_counter;
        } else if (key >= 0) {
          break;
        }
      }
      std::cout << std::endl;

      exit(EXIT_SUCCESS);
    } else { exit(EXIT_FAILURE); }
  } catch(exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch(...) {
    std::cerr << "some exceptions were thrown" << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

