#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/framerate_checker.h"
#include "filter_core/program_options.h"

#include <admxrc2.h>

#include <chrono>
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
using std::chrono::microseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using cv::Rect;
using filter_core::REFLESH_REG;
using filter_core::ENABLE_REG;
using filter_core::IMAGE_SIZE_REG;
using filter_core::FINISH_REG;
using filter_core::FPGACommunicator;
using filter_core::FramerateChecker;
using filter_core::GrayscaledCamera;
using filter_core::GetOptions;


namespace filter_core {
/*!
 * \var memory_clock_frequency
 * \brief メモリクロック
 */
constexpr double memory_clock_frequency = 66.67;
/*!
 * @var framerate_checker
 * \brief 出力ウィンドウタイトル
 */
constexpr const char* const frame_title = "filter";
}  // namespace filter_core

namespace {
/*!
 * \brief 画像を結合する
 * @param dst 出力先
 * @param filtered フィルタ画像
 * @param original 元画像
 * @param width 画像幅
 * @param height 画像高さ
 * @return 出力
 */
cv::Mat Combine(cv::Mat dst, cv::Mat filtered, cv::Mat original,
             uint64_t width, uint64_t height) {
  filtered.copyTo(cv::Mat(dst, Rect(0, 0, width, height)));
  original.copyTo(cv::Mat(dst, Rect(width, 0, width, height)));

  return dst;
}
/*!
 * \brief FPGAボードへrefresh信号を送る
 * @param com コミュニケータ
 */
void SendRefresh(FPGACommunicator& com) {
  com.write(filter_core::REFLESH_REG, 1);
  sleep_for(microseconds(200));
  com.write(filter_core::REFLESH_REG, 0);
  sleep_for(microseconds(100));
}
}  // namespace


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
  ::SendRefresh(communicator);
  communicator.write(ENABLE_REG, 1);

  std::cout << "waiting";
  for (int i = 0; i < 1000 && communicator[FINISH_REG] == 0; ++i) {
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

void testCam(cv::Size image_size, int interpolation) {
  auto start = system_clock::now();
 
  for (auto src : GrayscaledCamera(image_size, interpolation)) {
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



int main(int argc, char** argv) {
  try {
    auto options = GetOptions(argc, argv);

    if (options) {
      const cv::Size image_size = options->image_size;
      const uint32_t total_size = image_size.area();

      cv::Mat dst(image_size, CV_8UC1);
      cv::Mat combined({image_size.width * 2, image_size.height}, CV_8UC1);

      auto start = system_clock::now();

      const string output_prefix("output");
      uint64_t output_counter = 0;

      auto communicator = FPGACommunicator(
          options->frequency,
          filter_core::memory_clock_frequency,
          options->filename,
          total_size);

//      test(communicator, image_size, options->interpolation);

      // 画像サイズを指定
      communicator.write(filter_core::IMAGE_SIZE_REG, total_size);

      for (auto src : GrayscaledCamera(image_size, options->interpolation)) {
        // フレームレート計測
        FramerateChecker framerate_checker(start);

        // 画像を送信
        communicator.write(src.data, 0, total_size, 0);
        // refresh信号を送り、enableを有効にする
        ::SendRefresh(communicator);
        communicator.write(ENABLE_REG, 1);
        // フィルタリング完了を待つ
        for (int i = 0; i < 1000 && communicator[FINISH_REG] == 0; ++i)
          { sleep_for(microseconds(250)); }
        // enableを無効にする
        communicator.write(ENABLE_REG, 0);
        // 画像を取得
        communicator.read(dst.data, 0, total_size, 1);

        // 出力
        cv::Mat output = (options->is_with_captured)?
          ::Combine(combined, dst, src, image_size.width, image_size.height) :
          dst;
        cv::imshow(filter_core::frame_title, output);

        auto key = cv::waitKey(30);
        if (key == 'p' || key == 'P') {
          cv::imwrite(
              output_prefix + to_string(output_counter) + ".png", output);

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

