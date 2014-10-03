#include "filter_core/camera.h"
#include "filter_core/fpga_communicator.h"
#include "filter_core/framerate_checker.h"
#include "filter_core/program_options.h"

#include <admxrc2.h>
#include <opencv2/opencv.hpp>

#include <chrono>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iostream>
#include <string>
#include <thread>


using std::string;
using std::to_string;
using std::chrono::microseconds;
using std::chrono::system_clock;
using std::this_thread::sleep_for;
using cv::Rect;


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
                cv::Size size) {
  filtered.copyTo(cv::Mat(dst, Rect(0, 0, size.width, size.height)));
  original.copyTo(cv::Mat(dst, Rect(size.width, 0, size.width, size.height)));

  return dst;
}
/*!
 * \brief ハードウェアを用いて空間フィルタをかける.
 * @param com FPGAボードとのコミュニケータ
 */
void Filter(filter_core::FPGACommunicator& com,
            cv::Mat src, cv::Mat dst,
            uint32_t total_size,
            int wait_limit) {
  // 画像を送信
  com.write(src.data, 0, total_size, 0);
  // refresh信号を送り、enable信号を有効にする
  SendRefresh(com);
  com.write(ENABLE_REG, 1);
  // フィルタリング完了を待つ
  for (int i = 0; i < wait_limit && com[FINISH_REG] == 0; ++i)
    { sleep_for(microseconds(250)); }
  // enableを無効にする
  com.write(ENABLE_REG, 0);
  // 画像を取得
  com.read(dst.data, 0, total_size, 1);
}
/*!
 * \brief mainの実装.
 * @param options プログラム引数の解析結果
 * @return EXIT_SUCCESS
 */
int MainImpl(filter_core::Options&& options) {
  if (options.is_debug_mode) { ShowOptions(options); }

  const auto& image_options = options.image_options;

  cv::Mat dst(image_options.size, image_options.type);
  cv::Mat combined(image_options.combined_image_size, image_options.type);

  auto start = system_clock::now();

  const string output_prefix("output");
  uint64_t output_counter = 0;

  FPGACommunicator communicator(
      options.frequency,
      memory_clock_frequency,
      options.filename,
      image_options.total_size);

//      filter_core::test(communicator, image_size, options->interpolation);

  // 画像サイズを指定
  SendImageSize(communicator,
                image_options.total_size, image_options.width);

  for (auto src :
       Camera(
           Converter(image_options.size, image_options.type,
                     image_options.conversion,
                     image_options.interpolation))) {

    if (options.is_debug_mode) {
      Filter(
          // ユーザレジスタを表示
          OutputUserRegisters(communicator),
          src, dst, image_options.total_size, 1000);
    } else {
      // フレームレート計測
      FramerateChecker framerate_checker(start);

      Filter(communicator, src, dst, image_options.total_size, 1000);
    }

    // 出力
    cv::Mat output = (options.is_with_captured)?
      Combine(combined, dst, src, image_options.size) : dst;
    cv::imshow(frame_title, output);

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

  return EXIT_SUCCESS;
}
}  // namespace filter_core


int main(int argc, char** argv) {
  try {
    if (auto options = filter_core::GetOptions(argc, argv))
      { std::exit(filter_core::MainImpl(std::move(*options))); }
    else { std::exit(EXIT_FAILURE); }
  } catch(std::exception& e) {
    std::cerr << e.what() << std::endl;
    exit(EXIT_FAILURE);
  } catch(...) {
    std::cerr << "some exceptions were thrown" << std::endl;
    exit(EXIT_FAILURE);
  }
  return 0;
}

