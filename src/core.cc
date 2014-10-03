#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/framerate_checker.h"
#include "filter_core/program_options.h"

#include <admxrc2.h>
#include <opencv2/opencv.hpp>

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
using filter_core::ENABLE_REG;
using filter_core::IMAGE_SIZE_REG;
using filter_core::FINISH_REG;
using filter_core::Camera;
using filter_core::Converter;
using filter_core::FPGACommunicator;
using filter_core::FramerateChecker;
using filter_core::GetOptions;
using filter_core::SendRefresh;


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


namespace filter_core {
void test(filter_core::FPGACommunicator& com, cv::Size size, int inter);
}  // namespace filter_core


namespace filter_core {
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
}  // namespace filter_core


int main(int argc, char** argv) {
  try {
    auto options = GetOptions(argc, argv);

    if (options) {
      const cv::Size image_size = options->image_size;
      const uint32_t total_size = image_size.area() * 4;
      const int image_type = CV_8UC4;

      cv::Mat dst(image_size, image_type);
      cv::Mat combined({image_size.width * 2, image_size.height}, image_type);

      auto start = system_clock::now();

      const string output_prefix("output");
      uint64_t output_counter = 0;

      FPGACommunicator communicator(
          options->frequency,
          filter_core::memory_clock_frequency,
          options->filename,
          total_size);

//      filter_core::test(communicator, image_size, options->interpolation);

      // 画像サイズを指定
      SendImageSize(communicator, total_size, image_size.width);

      for (auto src :
           Camera(
               Converter(image_size, image_type,
                         CV_BGR2BGRA, options->interpolation))) {
        // フレームレート計測
        FramerateChecker framerate_checker(start);

        // 画像を送信
        communicator.write(src.data, 0, total_size, 0);
        // refresh信号を送り、enableを有効にする
        SendRefresh(communicator);
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
          ::filter_core::Combine(combined, dst, src,
                                 image_size.width, image_size.height) :
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

